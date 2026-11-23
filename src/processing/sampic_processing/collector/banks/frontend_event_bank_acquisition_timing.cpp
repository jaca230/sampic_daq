#include "processing/sampic_processing/collector/banks/frontend_event_bank_acquisition_timing.h"
#include <bit>
#include <stdexcept>

static_assert(std::endian::native == std::endian::little);
FrontendEventBankAcquisitionTiming::FrontendEventBankAcquisitionTiming(
    const SampicEvent& parent, std::uint64_t acquisition_id, std::size_t hit_count) {
    setBankPrefix("SA");
    const auto& t = parent.timing();
    if (t.prepare.count() < 0 || t.read.count() < 0 || t.decode.count() < 0 || t.total.count() < 0)
        throw std::invalid_argument("negative acquisition duration");
    record_.acquisition_id = acquisition_id;
    record_.readout_end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        parent.timestamp().time_since_epoch()).count();
    record_.hit_count = hit_count;
    record_.duration = {static_cast<std::uint64_t>(t.prepare.count()),
                        static_cast<std::uint64_t>(t.read.count()),
                        static_cast<std::uint64_t>(t.decode.count()),
                        static_cast<std::uint64_t>(t.total.count())};
    record_.retries = t.acquisition_retries;
}
