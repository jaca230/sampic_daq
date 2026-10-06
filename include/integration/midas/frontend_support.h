#ifndef SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_SUPPORT_H
#define SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_SUPPORT_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>

#include "midas.h"

class FrontendEvent;
class FrontendEventBuffer;

namespace frontend::runtime {
class Runtime;
}

namespace integration::midas {

/**
 * MIDAS-specific event composition and end-of-run delivery.
 *
 * The normal writer cannot run during an end-of-run transition because mfe
 * disables readout before invoking end_of_run(). These helpers synchronously
 * deliver both events already in the mfe ring and events produced while the
 * SAMPIC processing pipeline is drained.
 */
class FrontendSupport final {
public:
    static INT composeEvent(
        char* destination,
        const std::shared_ptr<FrontendEvent>& event,
        frontend::runtime::Runtime& runtime);

    static bool writeEventToRing(
        const std::shared_ptr<FrontendEvent>& event,
        EQUIPMENT& equipment,
        int ring_buffer_handle,
        frontend::runtime::Runtime& runtime,
        const std::atomic<bool>& stop_requested,
        std::chrono::milliseconds max_wait =
            std::chrono::milliseconds{1'000});

    static INT flushEndOfRun(
        FrontendEventBuffer& frontend_buffer,
        EQUIPMENT& equipment,
        void* event_buffer,
        int ring_buffer_handle,
        frontend::runtime::Runtime& runtime,
        std::size_t max_events = 10'000);

private:
    static INT flushRingBuffer(
        EQUIPMENT& equipment,
        int ring_buffer_handle,
        std::size_t max_events,
        std::size_t& events_sent);

    static INT flushFrontendBuffer(
        FrontendEventBuffer& frontend_buffer,
        EQUIPMENT& equipment,
        void* event_buffer,
        frontend::runtime::Runtime& runtime,
        std::size_t max_events,
        std::size_t& events_sent);

    static void recordSentEvent(EQUIPMENT& equipment, DWORD event_size);
};

}  // namespace integration::midas

#endif  // SAMPIC_DAQ_INTEGRATION_MIDAS_FRONTEND_SUPPORT_H
