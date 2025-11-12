#include "processing/sampic_processing/collector/banks/frontend_event_bank_event_timing.h"
#include <algorithm>

FrontendEventBankEventTiming::FrontendEventBankEventTiming(
    std::chrono::steady_clock::time_point fe_ts,
    uint32_t nhits,
    const std::vector<SampicEvent*>& parents,
    const std::string& prefix)
{
    setBankPrefix(prefix);

    record_.fe_timestamp_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            fe_ts.time_since_epoch()).count());
    record_.nhits    = nhits;
    record_.nparents = static_cast<uint32_t>(parents.size());

    uint32_t prep_max = 0, read_max = 0, dec_max = 0, tot_max = 0, retry_max = 0;
    uint64_t prep_sum = 0, read_sum = 0, dec_sum = 0, tot_sum = 0, retry_sum = 0;

    for (const auto& p : parents) {
        if (!p) continue;
        const auto& t = p->timing();
        const uint32_t prep = static_cast<uint32_t>(t.prepare.count());
        const uint32_t read = static_cast<uint32_t>(t.read.count());
        const uint32_t dec  = static_cast<uint32_t>(t.decode.count());
        const uint32_t tot  = static_cast<uint32_t>(t.total.count());
        const uint32_t retries = t.acquisition_retries;

        prep_sum += prep; read_sum += read; dec_sum += dec; tot_sum += tot;
        retry_sum += retries;
        prep_max = std::max(prep_max, prep);
        read_max = std::max(read_max, read);
        dec_max  = std::max(dec_max, dec);
        tot_max  = std::max(tot_max, tot);
        retry_max = std::max(retry_max, retries);
    }

    record_.sp_prepare_us_sum = static_cast<uint32_t>(prep_sum);
    record_.sp_read_us_sum    = static_cast<uint32_t>(read_sum);
    record_.sp_decode_us_sum  = static_cast<uint32_t>(dec_sum);
    record_.sp_total_us_sum   = static_cast<uint32_t>(tot_sum);

    record_.sp_prepare_us_max = prep_max;
    record_.sp_read_us_max    = read_max;
    record_.sp_decode_us_max  = dec_max;
    record_.sp_total_us_max   = tot_max;
    record_.sp_acq_retry_max  = retry_max;
    record_.sp_acq_retry_sum  = static_cast<uint32_t>(retry_sum);
}

const uint8_t* FrontendEventBankEventTiming::data() const {
    return reinterpret_cast<const uint8_t*>(&record_);
}

size_t FrontendEventBankEventTiming::size() const {
    return sizeof(Record);
}
