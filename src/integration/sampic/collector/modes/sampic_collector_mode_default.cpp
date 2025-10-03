#include "integration/sampic/collector/modes/sampic_collector_mode_default.h"
#include <spdlog/spdlog.h>
#include <thread>   // for sleep_for

int SampicCollectorModeDefault::readEvent(EventStruct& event,
                                          SampicEventTiming& timing) {
    auto t_start = std::chrono::steady_clock::now();
    SAMPIC256CH_PrepareEvent(&info_, &params_);
    auto t_after_prepare = std::chrono::steady_clock::now();

    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_NoFrameRead;
    int numberOfHits = 0;
    int nframes = 0;
    int dummy = 0;
    int nloop_for_soft_trig = 0;

    while (errCode != SAMPIC256CH_Success) {
        auto t_read_start = std::chrono::steady_clock::now();
        errCode = SAMPIC256CH_ReadEventBuffer(&info_, dummy, eventBuffer_, mlFrames_, &nframes);
        auto t_read_end = std::chrono::steady_clock::now();

        if (errCode == SAMPIC256CH_Success) {
            auto t_decode_start = std::chrono::steady_clock::now();
            errCode = SAMPIC256CH_DecodeEvent(&info_, &params_, mlFrames_, &event, nframes, &numberOfHits);
            auto t_decode_end = std::chrono::steady_clock::now();

            timing.decode_duration =
                std::chrono::duration_cast<std::chrono::microseconds>(t_decode_end - t_decode_start);
        }

        timing.read_duration +=
            std::chrono::duration_cast<std::chrono::microseconds>(t_read_end - t_read_start);

        if (errCode == SAMPIC256CH_AcquisitionError || errCode == SAMPIC256CH_ErrInvalidEvent) {
            spdlog::error("Default mode: Acquisition error (err={})", static_cast<int>(errCode));
            return -1;
        }

        if ((nloop_for_soft_trig % cfg_.soft_trigger_prepare_interval) == 0) {
            SAMPIC256CH_PrepareEvent(&info_, &params_);
        }
        nloop_for_soft_trig++;

        if (nloop_for_soft_trig > cfg_.soft_trigger_max_loops) {
            spdlog::warn("Default mode: Timeout after {} loops", cfg_.soft_trigger_max_loops);
            return 0;
        }

        // NEW: avoid pegging CPU if repeated failures
        if (errCode != SAMPIC256CH_Success && cfg_.soft_trigger_retry_sleep_us > 0) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(cfg_.soft_trigger_retry_sleep_us));
        }
    }

    timing.prepare_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(t_after_prepare - t_start);

    if (errCode == SAMPIC256CH_Success && numberOfHits > 0) {
        spdlog::debug("Default mode: Collected {} hits (prepare={}us, read={}us, decode={}us)",
                      numberOfHits,
                      timing.prepare_duration.count(),
                      timing.read_duration.count(),
                      timing.decode_duration.count());
    }

    return numberOfHits;
}
