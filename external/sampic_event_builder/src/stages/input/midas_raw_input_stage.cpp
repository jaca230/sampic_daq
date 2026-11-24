#include "sampic_event_builder/stages/input/midas_raw_input_stage.h"

#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace sampic::event_builder {
namespace {

using BankHeader = wire::RawBankHeader;
using RawHit = HitStruct;
static_assert(std::is_trivially_copyable_v<RawHit>);
static_assert(MAX_NB_OF_SAMPLES==64);

std::uint16_t crate_from_name(std::string_view name) {
    if (name.size() != 4 || name[0] != 'S' || name[1] != 'R' ||
        name[2] < '0' || name[2] > '9' ||
        name[3] < '0' || name[3] > '9') {
        throw std::invalid_argument("raw SAMPIC bank name must match SR[0-9][0-9]");
    }
    return static_cast<std::uint16_t>(
        (name[2] - '0') * 10 + (name[3] - '0'));
}

template <typename T>
T read_at(std::span<const std::uint8_t> payload, std::size_t offset) {
    if (offset > payload.size() || payload.size() - offset < sizeof(T)) {
        throw std::runtime_error("truncated raw SAMPIC bank");
    }
    T value{};
    std::memcpy(&value, payload.data() + offset, sizeof(T));
    return value;
}

std::uint64_t timestamp(const RawHit& record) {
    const double value = record.FirstCellTimeStamp != 0.0
        ? record.FirstCellTimeStamp
        : record.TimeInstant;
    if (!std::isfinite(value)) throw std::runtime_error("nonfinite SAMPIC grouping timestamp");
    if (value <= 0.0) {
        return 0;
    }
    if (value >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(value);
}

} // namespace

void MidasRawInputStage::ingest(
    std::string_view bank_name,
    std::span<const std::uint8_t> payload) {
    std::vector<Hit> decoded;
    try {
        const auto crate = crate_from_name(bank_name);
        const auto header = read_at<BankHeader>(payload, 0);
        if (header.magic != wire::raw_magic || header.version != wire::raw_version ||
            header.header_size != sizeof(BankHeader) || header.record_size != sizeof(RawHit))
            throw std::runtime_error("unsupported raw SAMPIC bank header");
        if (header.hit_count > (payload.size()-sizeof(BankHeader))/sizeof(RawHit) ||
            payload.size()-sizeof(BankHeader) != static_cast<std::size_t>(header.hit_count)*sizeof(RawHit))
            throw std::runtime_error("invalid raw SAMPIC bank length/count");
        // One ownership/copy operation for the entire bank. MIDAS may reuse
        // its receive buffer immediately after ingest returns. Each Hit shares
        // this batch's control block while pointing at its own vendor record.
        auto batch=std::shared_ptr<RawHit[]>(new RawHit[header.hit_count]);
        if (header.hit_count)
            std::memcpy(batch.get(),payload.data()+header.header_size,header.hit_count*sizeof(RawHit));
        decoded.reserve(header.hit_count);
        for (std::uint32_t index=0; index<header.hit_count; ++index) {
            const auto& record=batch[index];
            wire::validate_hit(record);
            Hit hit;
            hit.crate=crate;
            hit.board=static_cast<std::uint16_t>(record.FeBoardIndex);
            hit.channel=static_cast<std::uint16_t>(record.Channel);
            hit.timestamp_ns=timestamp(record);
            hit.raw=std::shared_ptr<const RawHit>(batch,&record);
            decoded.push_back(std::move(hit));
        }
    } catch (...) {
        std::lock_guard lock(mutex_);
        ++malformed_banks_;
        throw;
    }

    std::lock_guard lock(mutex_);
    ++banks_received_;
    hits_received_ += decoded.size();
    bytes_received_ += payload.size();
    pending_.insert(
        pending_.end(),
        std::make_move_iterator(decoded.begin()),
        std::make_move_iterator(decoded.end()));
}

std::vector<Hit> MidasRawInputStage::read_available() {
    std::lock_guard lock(mutex_);
    std::vector<Hit> result;
    result.swap(pending_);
    return result;
}

std::uint64_t MidasRawInputStage::banks_received() const {
    std::lock_guard lock(mutex_);
    return banks_received_;
}

std::uint64_t MidasRawInputStage::malformed_banks() const {
    std::lock_guard lock(mutex_);
    return malformed_banks_;
}

std::uint64_t MidasRawInputStage::hits_received() const {
    std::lock_guard lock(mutex_);
    return hits_received_;
}

std::uint64_t MidasRawInputStage::bytes_received() const {
    std::lock_guard lock(mutex_);
    return bytes_received_;
}

void MidasRawInputStage::reset_counters() {
    std::lock_guard lock(mutex_);
    banks_received_ = 0;
    hits_received_ = 0;
    bytes_received_ = 0;
    malformed_banks_ = 0;
}

} // namespace sampic::event_builder
