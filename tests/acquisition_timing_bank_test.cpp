#include "processing/sampic_processing/collector/banks/frontend_event_bank_acquisition_timing.h"
#include <cassert>
#include <cstring>
#include <stdexcept>
int main() {
    SampicEvent parent;
    parent.setTimestamp(std::chrono::steady_clock::time_point(std::chrono::nanoseconds(1234)));
    parent.setTiming({std::chrono::microseconds(2), std::chrono::microseconds(3),
        std::chrono::microseconds(4), std::chrono::microseconds(9), 5});
    FrontendEventBankAcquisitionTiming bank(parent, 7, 64);
    assert(bank.size() == 72);
    sampic::event_builder::timing::Acquisition record;
    std::memcpy(&record, bank.data(), sizeof(record));
    assert(std::memcmp(record.magic, "SAT1", 4) == 0 && record.version == 1 && record.record_size == 72);
    assert(record.acquisition_id == 7 && record.readout_end_ns == 1234 && record.hit_count == 64);
    assert(record.duration.prepare_us == 2 && record.duration.read_us == 3);
    assert(record.duration.decode_us == 4 && record.duration.total_us == 9 && record.retries == 5);
    parent.setTiming({std::chrono::microseconds(-1)});
    bool rejected = false;
    try { FrontendEventBankAcquisitionTiming invalid(parent, 8, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
