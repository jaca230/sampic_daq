#pragma once

#include "sampic_event_builder/stages/input/input_stage.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace sampic::event_builder {

/// Decodes version-1 SRnn banks emitted by vendor_raw_passthrough collectors.
/// MIDAS owns the source event memory, so decoded hits are copied into a local
/// queue before the callback returns.
class MidasRawInputStage final : public InputStage {
public:
    void ingest(std::string_view bank_name,
                std::span<const std::uint8_t> payload);
    std::vector<Hit> read_available() override;
    std::string_view type() const override { return "midas-raw"; }

    std::uint64_t banks_received() const;
    std::uint64_t hits_received() const;
    std::uint64_t bytes_received() const;
    std::uint64_t malformed_banks() const;
    void reset_counters();

private:
    mutable std::mutex mutex_;
    std::vector<Hit> pending_;
    std::uint64_t banks_received_{0};
    std::uint64_t hits_received_{0};
    std::uint64_t bytes_received_{0};
    std::uint64_t malformed_banks_{0};
};

} // namespace sampic::event_builder
