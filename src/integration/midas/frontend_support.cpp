#include "integration/midas/frontend_support.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "mfe.h"

#include <spdlog/spdlog.h>

#include "integration/midas/frontend_runtime.h"
#include "processing/sampic_processing/collector/frontend_event.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"

// Ring-buffer declarations live in MIDAS's msystem.h, but that header also
// defines localtime/gmtime poison macros which conflict with spdlog headers.
// Keep this translation unit on the narrow public ABI used by mfe instead.
int rb_get_wp(int handle, void** pointer, int timeout_ms);
int rb_increment_wp(int handle, int size);
int rb_get_rp(int handle, void** pointer, int timeout_ms);
int rb_increment_rp(int handle, int size);
int rb_get_buffer_level(int handle, int* bytes);

namespace integration::midas {

INT FrontendSupport::composeEvent(
    char* destination,
    const std::shared_ptr<FrontendEvent>& event,
    frontend::runtime::Runtime& runtime) {
    if (!destination || !event) {
        return 0;
    }

    spdlog::trace(
        "Composing FrontendEvent with {} bank(s)", event->numBanks());

    bk_init32(destination);

    std::size_t bank_index = 0;
    for (const auto& bank : event->banks()) {
        if (!bank) {
            continue;
        }

        const std::string bank_name =
            runtime.makeBankName(bank->bankPrefix());
        std::uint8_t* data = nullptr;
        bk_create(
            destination, bank_name.c_str(), TID_UINT8, (void**)&data);
        std::uint8_t* const start = data;

        bank->writeTo(data);
        data += bank->size();

        bk_close(destination, data);
        spdlog::trace(
            "FrontendEvent bank[{}] -> wrote {} ({} bytes)",
            bank_index++,
            bank_name,
            static_cast<int>(data - start));
    }

    return bk_size(destination);
}

bool FrontendSupport::writeEventToRing(
    const std::shared_ptr<FrontendEvent>& frontend_event,
    EQUIPMENT& equipment,
    int ring_buffer_handle,
    frontend::runtime::Runtime& runtime,
    const std::atomic<bool>& stop_requested,
    std::chrono::milliseconds max_wait) {
    if (!frontend_event || ring_buffer_handle == 0) {
        return false;
    }

    EVENT_HEADER* event = nullptr;
    const auto deadline = std::chrono::steady_clock::now() + max_wait;
    while (!stop_requested.load() &&
           std::chrono::steady_clock::now() < deadline) {
        const INT status =
            rb_get_wp(ring_buffer_handle, (void**)&event, 0);
        if (status == DB_SUCCESS) {
            break;
        }
        if (status != DB_TIMEOUT) {
            spdlog::error(
                "MIDAS ring-buffer write-pointer lookup failed with "
                "status={}",
                status);
            return false;
        }
        std::this_thread::yield();
    }

    if (!event) {
        if (!stop_requested.load()) {
            spdlog::error(
                "Timed out waiting for MIDAS ring-buffer space");
        }
        return false;
    }

    const DWORD serial_before = equipment.serial_number;
    bm_compose_event_threadsafe(
        event,
        equipment.info.event_id,
        equipment.info.trigger_mask,
        0,
        &equipment.serial_number);

    auto* payload = reinterpret_cast<char*>(event + 1);
    const INT data_size = composeEvent(payload, frontend_event, runtime);
    if (data_size <= 0) {
        equipment.serial_number = serial_before;
        spdlog::error("Failed to compose FrontendEvent for MIDAS ring");
        return false;
    }
    event->data_size = data_size;

    const INT increment_status = rb_increment_wp(
        ring_buffer_handle,
        sizeof(EVENT_HEADER) + event->data_size);
    if (increment_status != DB_SUCCESS) {
        equipment.serial_number = serial_before;
        spdlog::error(
            "MIDAS ring-buffer write commit failed with status={}",
            increment_status);
        return false;
    }

    return true;
}

INT FrontendSupport::flushEndOfRun(
    FrontendEventBuffer& frontend_buffer,
    EQUIPMENT& equipment,
    void* event_buffer,
    int ring_buffer_handle,
    frontend::runtime::Runtime& runtime,
    std::size_t max_events) {
    std::size_t ring_events_sent = 0;
    INT status = flushRingBuffer(
        equipment,
        ring_buffer_handle,
        max_events,
        ring_events_sent);
    if (status != SUCCESS) {
        return status;
    }

    const std::size_t remaining_limit =
        max_events > ring_events_sent
            ? max_events - ring_events_sent
            : 0;
    std::size_t frontend_events_sent = 0;
    status = flushFrontendBuffer(
        frontend_buffer,
        equipment,
        event_buffer,
        runtime,
        remaining_limit,
        frontend_events_sent);
    if (status != SUCCESS) {
        return status;
    }

    status = rpc_flush_event();
    if (status != RPC_SUCCESS) {
        spdlog::error(
            "End-of-run rpc_flush_event failed with status={}", status);
        return status;
    }

    if (equipment.buffer_handle) {
        status = bm_flush_cache(equipment.buffer_handle, BM_WAIT);
        if (status != BM_SUCCESS) {
            spdlog::error(
                "End-of-run bm_flush_cache failed with status={}", status);
            return status;
        }
    }

    spdlog::info(
        "End-of-run flush sent {} ring-buffer event(s) and {} "
        "newly drained frontend event(s)",
        ring_events_sent,
        frontend_events_sent);
    return SUCCESS;
}

INT FrontendSupport::flushRingBuffer(
    EQUIPMENT& equipment,
    int ring_buffer_handle,
    std::size_t max_events,
    std::size_t& events_sent) {
    events_sent = 0;
    if (ring_buffer_handle == 0) {
        return SUCCESS;
    }

    while (events_sent < max_events) {
        void* read_pointer = nullptr;
        const INT read_status =
            rb_get_rp(ring_buffer_handle, &read_pointer, 0);
        if (read_status == DB_TIMEOUT) {
            return SUCCESS;
        }
        if (read_status != DB_SUCCESS || !read_pointer) {
            spdlog::error(
                "End-of-run rb_get_rp failed with status={}",
                read_status);
            return FE_ERR_HW;
        }

        auto* event = static_cast<EVENT_HEADER*>(read_pointer);
        if (event->serial_number != equipment.events_collected) {
            spdlog::error(
                "End-of-run ring serial mismatch: expected {}, found {}",
                equipment.events_collected,
                event->serial_number);
            return FE_ERR_HW;
        }

        const DWORD event_size =
            sizeof(EVENT_HEADER) + event->data_size;
        if (event->data_size && equipment.buffer_handle) {
            const INT send_status = rpc_send_event(
                equipment.buffer_handle,
                event,
                event_size,
                BM_WAIT,
                rpc_mode);
            if (send_status != SUCCESS) {
                spdlog::error(
                    "End-of-run rpc_send_event for ring event failed "
                    "with status={}",
                    send_status);
                return send_status;
            }

            recordSentEvent(equipment, event_size);
        }

        const INT increment_status =
            rb_increment_rp(ring_buffer_handle, event_size);
        if (increment_status != DB_SUCCESS) {
            spdlog::error(
                "End-of-run rb_increment_rp failed with status={}",
                increment_status);
            return FE_ERR_HW;
        }
        ++events_sent;
    }

    int bytes_remaining = 0;
    const INT level_status =
        rb_get_buffer_level(ring_buffer_handle, &bytes_remaining);
    if (level_status != DB_SUCCESS) {
        spdlog::error(
            "End-of-run rb_get_buffer_level failed with status={}",
            level_status);
        return FE_ERR_HW;
    }
    if (bytes_remaining != 0) {
        spdlog::error(
            "End-of-run flush exceeded its {} event limit with {} "
            "ring-buffer byte(s) remaining",
            max_events,
            bytes_remaining);
        return FE_ERR_HW;
    }

    return SUCCESS;
}

INT FrontendSupport::flushFrontendBuffer(
    FrontendEventBuffer& frontend_buffer,
    EQUIPMENT& equipment,
    void* event_buffer,
    frontend::runtime::Runtime& runtime,
    std::size_t max_events,
    std::size_t& events_sent) {
    events_sent = 0;
    if (!event_buffer) {
        spdlog::error("MIDAS event buffer is unavailable during end-of-run");
        return FE_ERR_HW;
    }

    while (!frontend_buffer.empty()) {
        if (events_sent >= max_events) {
            spdlog::error(
                "End-of-run flush exceeded its frontend event limit "
                "with {} event(s) remaining",
                frontend_buffer.size());
            return FE_ERR_HW;
        }

        auto popped = frontend_buffer.pop();
        if (!popped || !*popped) {
            continue;
        }
        const auto& frontend_event = *popped;

        auto* event = static_cast<EVENT_HEADER*>(event_buffer);
        const DWORD serial_before = equipment.serial_number;
        bm_compose_event_threadsafe(
            event,
            equipment.info.event_id,
            equipment.info.trigger_mask,
            0,
            &equipment.serial_number);

        auto* payload = reinterpret_cast<char*>(event + 1);
        const INT data_size =
            composeEvent(payload, frontend_event, runtime);
        if (data_size <= 0) {
            equipment.serial_number = serial_before;
            frontend_buffer.pushFront(frontend_event);
            spdlog::error(
                "Failed to compose a drained frontend event at end-of-run");
            return FE_ERR_HW;
        }
        event->data_size = data_size;

        const DWORD total_size =
            sizeof(EVENT_HEADER) + event->data_size;
        if (!equipment.buffer_handle) {
            equipment.serial_number = serial_before;
            frontend_buffer.pushFront(frontend_event);
            spdlog::error(
                "MIDAS output buffer is unavailable during end-of-run");
            return FE_ERR_HW;
        }

        const INT send_status = rpc_send_event(
            equipment.buffer_handle,
            event,
            total_size,
            BM_WAIT,
            rpc_mode);
        if (send_status != SUCCESS) {
            equipment.serial_number = serial_before;
            frontend_buffer.pushFront(frontend_event);
            spdlog::error(
                "End-of-run rpc_send_event failed with status={}",
                send_status);
            return send_status;
        }

        frontend_event->markConsumed(true);
        runtime.lastEventTimestamp = frontend_event->timestamp();
        if (runtime.collector) {
            runtime.collector->diagnostics().consumed(
                1, frontend_buffer.size());
        }
        recordSentEvent(equipment, total_size);
        ++events_sent;
    }

    return SUCCESS;
}

void FrontendSupport::recordSentEvent(
    EQUIPMENT& equipment,
    DWORD event_size) {
    equipment.bytes_sent += event_size;
    if (equipment.info.num_subevents) {
        equipment.events_sent += equipment.subevent_number;
    } else {
        ++equipment.events_sent;
    }
    ++equipment.events_collected;
}

}  // namespace integration::midas
