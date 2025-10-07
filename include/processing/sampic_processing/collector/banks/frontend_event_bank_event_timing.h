#ifndef FRONTEND_EVENT_BANK_EVENT_TIMING_H
#define FRONTEND_EVENT_BANK_EVENT_TIMING_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include "integration/sampic/collector/sampic_event.h"

#include <chrono>
#include <vector>
#include <memory>
#include <cstdint>

/**
 * @class FrontendEventBankEventTiming
 * @brief Encodes timing metrics for a single FrontendEvent.
 *
 * Computes aggregate and maximum SAMPIC processing times across all
 * contributing parent SampicEvents.
 */
class FrontendEventBankEventTiming : public FrontendEventBank {
public:
#pragma pack(push, 1)
    struct Record {
        /** Timestamp of the frontend event in nanoseconds. */
        uint64_t fe_timestamp_ns;

        /** Number of SAMPIC hits and parent events in the group. */
        uint32_t nhits;
        uint32_t nparents;

        /** Aggregated SAMPIC timings (microseconds). */
        uint32_t sp_prepare_us_sum;
        uint32_t sp_read_us_sum;
        uint32_t sp_decode_us_sum;
        uint32_t sp_total_us_sum;

        /** Maximum SAMPIC timings (microseconds). */
        uint32_t sp_prepare_us_max;
        uint32_t sp_read_us_max;
        uint32_t sp_decode_us_max;
        uint32_t sp_total_us_max;
    };
#pragma pack(pop)

    /**
     * @brief Construct directly from group context.
     * @param fe_ts Timestamp of the frontend event.
     * @param nhits Number of hits in this event.
     * @param parents Vector of contributing SampicEvent objects.
     * @param prefix Optional bank prefix (default "AT").
     */
    FrontendEventBankEventTiming(std::chrono::steady_clock::time_point fe_ts,
                                 uint32_t nhits,
                                 const std::vector<std::shared_ptr<SampicEvent>>& parents,
                                 const std::string& prefix = "AT");

    /** @brief Return pointer to serialized record data. */
    const uint8_t* data() const override;

    /** @brief Return byte size of serialized record. */
    size_t size() const override;

private:
    Record record_{};
};

#endif // FRONTEND_EVENT_BANK_EVENT_TIMING_H
