#include "sampic_event_builder/stages/input/midas_raw_input_stage.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sampic::event_builder {
namespace {

constexpr std::uint32_t kMagic = 0x31575253U; // "SRW1"
constexpr std::uint16_t kVersion = 1;

#pragma pack(push, 1)
struct BankHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t header_size;
    std::uint32_t hit_count;
};

struct RawHitHeader {
    std::uint32_t record_size;
    std::int16_t fe_board_index;
    std::int16_t sampic_index;
    std::int16_t channel_index;
    std::int16_t channel;
    std::int32_t hit_number;
    std::int32_t cell_info;
    std::int32_t first_cell_physical_index;
    std::uint16_t sample_count;
    std::uint16_t reserved;
    double time_instant_ns;
    double first_cell_timestamp_ns;
};
#pragma pack(pop)

static_assert(sizeof(BankHeader) == 12);
static_assert(sizeof(RawHitHeader) == 44);

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

std::uint64_t timestamp(const RawHitHeader& record) {
    const double value = record.first_cell_timestamp_ns != 0.0
        ? record.first_cell_timestamp_ns
        : record.time_instant_ns;
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
        if (header.magic != kMagic || header.version != kVersion ||
            header.header_size < sizeof(BankHeader) ||
            header.header_size > payload.size()) {
            throw std::runtime_error("unsupported raw SAMPIC bank header");
        }

        std::size_t offset = header.header_size;
        decoded.reserve(header.hit_count);
        for (std::uint32_t index = 0; index < header.hit_count; ++index) {
            const auto record = read_at<RawHitHeader>(payload, offset);
            const std::size_t samples_size =
                static_cast<std::size_t>(record.sample_count) * sizeof(std::uint16_t);
            const std::size_t minimum_size = sizeof(RawHitHeader) + samples_size;
            if (record.record_size < minimum_size ||
                offset > payload.size() ||
                payload.size() - offset < record.record_size) {
                throw std::runtime_error("invalid raw SAMPIC hit record size");
            }

            Hit hit;
            hit.crate = crate;
            hit.board = static_cast<std::uint16_t>(record.fe_board_index);
            hit.channel = static_cast<std::uint16_t>(record.channel);
            hit.timestamp_ns = timestamp(record);
            hit.waveform.resize(record.sample_count);
            const std::uint8_t* samples =
                payload.data() + offset + sizeof(RawHitHeader);
            // SR banks use the host-native little-endian uint16 representation;
            // SAMPIC ADC values fit in int16, so the bitwise copy is lossless.
            std::memcpy(hit.waveform.data(), samples, samples_size);
            decoded.push_back(std::move(hit));
            offset += record.record_size;
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
