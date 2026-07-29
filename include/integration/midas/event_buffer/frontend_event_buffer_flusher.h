#ifndef SAMPIC_DAQ_INTEGRATION_MIDAS_EVENT_BUFFER_FRONTEND_EVENT_BUFFER_FLUSHER_H
#define SAMPIC_DAQ_INTEGRATION_MIDAS_EVENT_BUFFER_FRONTEND_EVENT_BUFFER_FLUSHER_H

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>

#include "midas.h"
#include "processing/sampic_processing/collector/frontend_event.h"
#include "processing/sampic_processing/collector/frontend_event_buffer.h"

namespace integration::midas {

class FrontendEventBufferFlusher {
public:
    using SerializeCallback = std::function<int(char*, const std::shared_ptr<FrontendEvent>&)>;
    using ConsumedCallback = std::function<void(size_t)>;

    explicit FrontendEventBufferFlusher(int readout_thread_index = 0);

    bool writeOne(const std::shared_ptr<FrontendEvent>& event,
                  int event_id,
                  int trigger_mask,
                  DWORD* serial_number,
                  std::atomic<bool>& stop_flag,
                  const SerializeCallback& serializer,
                  int max_wait_ms) const;

    INT flush(FrontendEventBuffer& buffer,
              size_t max_events,
              int max_wait_ms,
              int event_id,
              int trigger_mask,
              DWORD* serial_number,
              std::atomic<bool>& stop_flag,
              const SerializeCallback& serializer,
              const ConsumedCallback& consumed_cb,
              size_t& flushed_events) const;

private:
    int rbh_{-1};
};

}  // namespace integration::midas

#endif  // SAMPIC_DAQ_INTEGRATION_MIDAS_EVENT_BUFFER_FRONTEND_EVENT_BUFFER_FLUSHER_H
