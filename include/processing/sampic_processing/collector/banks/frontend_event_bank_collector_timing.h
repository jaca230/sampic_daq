#ifndef FRONTEND_EVENT_BANK_COLLECTOR_TIMING_H
#define FRONTEND_EVENT_BANK_COLLECTOR_TIMING_H

#include "processing/sampic_processing/collector/banks/frontend_event_bank.h"
#include <chrono>
#include <cstdint>

/**
 * @class FrontendEventBankCollectorTiming
 * @brief Records performance and timing metrics for one collector iteration.
 *
 * The record summarizes wait, grouping, finalization, and total durations
 * at the collector level. It is populated and passed in directly by the
 * collector mode.
 */
class FrontendEventBankCollectorTiming : public FrontendEventBank {
public:
#pragma pack(push, 1)
    struct Record {
        /** Timestamp (ns since epoch) when the collection cycle started. */
        uint64_t collector_timestamp_ns;

        /** Number of frontend events emitted in this collection cycle. */
        uint32_t n_events;

        /** Total number of hits across all emitted frontend events. */
        uint32_t total_hits;

        /** Collector wait duration in microseconds. */
        uint32_t wait_us;

        /** Duration of grouping phase in microseconds. */
        uint32_t group_build_us;

        /** Duration of frontend finalization in microseconds. */
        uint32_t finalize_us;

        /** Total end-to-end collection time in microseconds. */
        uint32_t total_us;
    };
#pragma pack(pop)

    /**
     * @brief Construct the collector timing bank directly from a filled record.
     * @param record Fully populated timing record.
     * @param prefix Optional bank prefix (default "AC").
     */
    explicit FrontendEventBankCollectorTiming(const Record& record,
                                              const std::string& prefix = "AC");

    /** @brief Return pointer to serialized record data. */
    const uint8_t* data() const override;

    /** @brief Return byte size of serialized record. */
    size_t size() const override;

private:
    Record record_{};
};

#endif // FRONTEND_EVENT_BANK_COLLECTOR_TIMING_H
