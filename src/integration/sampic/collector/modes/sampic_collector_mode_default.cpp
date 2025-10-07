#include "integration/sampic/collector/modes/sampic_collector_mode_default.h"
#include "integration/sampic/collector/sampic_event.h"

#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

SampicCollectorModeDefault::SampicCollectorModeDefault(
    SampicEventBuffer& buffer,
    CrateInfoStruct& info,
    CrateParamStruct& params,
    void* eventBuffer,
    ML_Frame* mlFrames,
    const SampicCollectorConfig& cfg)
    : SampicCollectorMode(buffer, info, params, eventBuffer, mlFrames, cfg),
      mode_cfg_(cfg.default_mode)
{
    spdlog::info("SAMPICCollectorModeDefault initialized: "
                 "soft_trigger_prepare_interval={}, "
                 "soft_trigger_max_loops={}, "
                 "soft_trigger_retry_sleep_us={}",
                 mode_cfg_.soft_trigger_prepare_interval,
                 mode_cfg_.soft_trigger_max_loops,
                 mode_cfg_.soft_trigger_retry_sleep_us);
}

bool SampicCollectorModeDefault::collect()
{
    SampicTimingBreakdown timing{};
    auto ev_data = std::make_shared<EventStruct>();

    const auto t_start = std::chrono::steady_clock::now();
    SAMPIC256CH_PrepareEvent(&info_, &params_);
    const auto t_after_prepare = std::chrono::steady_clock::now();

    SAMPIC256CH_ErrCode errCode = SAMPIC256CH_NoFrameRead;
    int numberOfHits = 0;
    int nframes = 0;
    int dummy = 0;
    int nloop = 0;

    // ---------------------------------------------------------------------
    // Acquisition loop
    // ---------------------------------------------------------------------
    while (errCode != SAMPIC256CH_Success)
    {
        const auto t_read_start = std::chrono::steady_clock::now();
        errCode = SAMPIC256CH_ReadEventBuffer(&info_, dummy, eventBuffer_, mlFrames_, &nframes);
        const auto t_read_end = std::chrono::steady_clock::now();

        timing.read += std::chrono::duration_cast<std::chrono::microseconds>(t_read_end - t_read_start);

        if (errCode == SAMPIC256CH_Success)
        {
            const auto t_decode_start = std::chrono::steady_clock::now();
            errCode = SAMPIC256CH_DecodeEvent(&info_, &params_, mlFrames_, ev_data.get(), nframes, &numberOfHits);
            const auto t_decode_end = std::chrono::steady_clock::now();
            timing.decode = std::chrono::duration_cast<std::chrono::microseconds>(t_decode_end - t_decode_start);
        }

        if (errCode == SAMPIC256CH_AcquisitionError || errCode == SAMPIC256CH_ErrInvalidEvent)
        {
            spdlog::error("SAMPIC default mode: acquisition error (errCode={})",
                          static_cast<int>(errCode));
            return false;
        }

        // Retry / prepare logic
        if ((nloop % mode_cfg_.soft_trigger_prepare_interval) == 0)
            SAMPIC256CH_PrepareEvent(&info_, &params_);

        ++nloop;

        if (nloop > mode_cfg_.soft_trigger_max_loops)
        {
            spdlog::warn("SAMPIC default mode: timeout after {} loops", nloop);
            return true;
        }

        if (errCode != SAMPIC256CH_Success && mode_cfg_.soft_trigger_retry_sleep_us > 0)
            std::this_thread::sleep_for(std::chrono::microseconds(mode_cfg_.soft_trigger_retry_sleep_us));
    }

    // ---------------------------------------------------------------------
    // Timing and event assembly
    // ---------------------------------------------------------------------
    timing.prepare = std::chrono::duration_cast<std::chrono::microseconds>(t_after_prepare - t_start);
    timing.total   = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now() - t_start);

    if (errCode == SAMPIC256CH_Success && numberOfHits > 0)
    {
        auto ev = std::make_shared<SampicEvent>(
            ev_data, timing, std::chrono::steady_clock::now());
        buffer_.push(ev);

        spdlog::debug("SAMPIC default mode: collected {} hits "
                      "(prepare={}us, read={}us, decode={}us, total={}us)",
                      numberOfHits,
                      timing.prepare.count(),
                      timing.read.count(),
                      timing.decode.count(),
                      timing.total.count());
    }

    return true;
}
