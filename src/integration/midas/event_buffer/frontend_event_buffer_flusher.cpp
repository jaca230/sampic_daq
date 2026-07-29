#include "integration/midas/event_buffer/frontend_event_buffer_flusher.h"

#include <chrono>
#include <thread>

#include "mfe.h"
#include <spdlog/spdlog.h>

int rb_get_wp(int handle, void** p, int millisec);
int rb_increment_wp(int handle, int size);

namespace integration::midas {

FrontendEventBufferFlusher::FrontendEventBufferFlusher(int readout_thread_index)
    : rbh_(get_event_rbh(readout_thread_index)) {}

bool FrontendEventBufferFlusher::writeOne(const std::shared_ptr<FrontendEvent>& event,
                                          int event_id,
                                          int trigger_mask,
                                          DWORD* serial_number,
                                          std::atomic<bool>& stop_flag,
                                          const SerializeCallback& serializer,
                                          int max_wait_ms) const
{
    if (!event || rbh_ < 0 || !serializer) {
        return false;
    }

    EVENT_HEADER* pevent = nullptr;
    int status = DB_TIMEOUT;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(max_wait_ms);

    while (std::chrono::steady_clock::now() < deadline && !stop_flag.load()) {
        status = rb_get_wp(rbh_, reinterpret_cast<void**>(&pevent), 0);
        if (status == DB_SUCCESS) {
            break;
        }
        if (status != DB_TIMEOUT) {
            spdlog::error("FrontendEventBufferFlusher::writeOne rb_get_wp failed with status={}", status);
            pevent = nullptr;
            break;
        }
        std::this_thread::yield();
    }

    if (!pevent) {
        spdlog::error("FrontendEventBufferFlusher::writeOne no ring-buffer write pointer available");
        return false;
    }

    bm_compose_event_threadsafe(pevent, event_id, trigger_mask, 0, serial_number);

    auto* payload = reinterpret_cast<char*>(pevent + 1);
    pevent->data_size = serializer(payload, event);
    rb_increment_wp(rbh_, sizeof(EVENT_HEADER) + pevent->data_size);
    return true;
}

INT FrontendEventBufferFlusher::flush(FrontendEventBuffer& buffer,
                                      size_t max_events,
                                      int max_wait_ms,
                                      int event_id,
                                      int trigger_mask,
                                      DWORD* serial_number,
                                      std::atomic<bool>& stop_flag,
                                      const SerializeCallback& serializer,
                                      const ConsumedCallback& consumed_cb,
                                      size_t& flushed_events) const
{
    flushed_events = 0;

    if (rbh_ < 0) {
        spdlog::error("FrontendEventBufferFlusher::flush invalid ring buffer handle");
        return FE_ERR_HW;
    }

    while (flushed_events < max_events) {
        auto opt_event = buffer.pop();
        if (!opt_event || !(*opt_event)) {
            return SUCCESS;
        }

        if (!writeOne(*opt_event,
                      event_id,
                      trigger_mask,
                      serial_number,
                      stop_flag,
                      serializer,
                      max_wait_ms)) {
            return FE_ERR_HW;
        }

        ++flushed_events;
        if (consumed_cb) {
            consumed_cb(buffer.size());
        }
    }

    spdlog::error("FrontendEventBufferFlusher::flush reached flush limit ({})", max_events);
    return FE_ERR_HW;
}

}  // namespace integration::midas
