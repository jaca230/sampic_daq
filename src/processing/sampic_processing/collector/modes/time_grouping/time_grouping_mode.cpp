#include "processing/sampic_processing/collector/modes/time_grouping/time_grouping_mode.h"

#include "core/registry/mode/mode_auto_registration.h"

#include <stdexcept>
#include <utility>

namespace {
FrontendCollectorModeDefaultConfig groupingConfig(
    const FrontendCollectorModeTimeGroupingConfig& config) {
    return {
        config.time_window_ns,
        config.finalize_after_ms,
        config.wait_timeout_ms,
        config.data_bank_enabled,
        config.data_bank_prefix,
        config.event_timing_bank_enabled,
        config.event_timing_bank_prefix,
        config.collector_timing_bank_enabled,
        config.collector_timing_bank_prefix,
    };
}
}

SAMPIC_REGISTER_MODE(
    FrontendCollectorModeRegistry,
    FrontendCollectorModeTimeGrouping,
    FrontendCollectorModeTimeGroupingConfig,
    "time_grouping",
    "Hit-time clustering with optional advanced-hit data",
    [](const FrontendCollectorModeTimeGroupingConfig& config) {
        if (config.time_window_ns < 0 || config.finalize_after_ms < 0 ||
            config.wait_timeout_ms == 0) {
            throw std::invalid_argument("timing values are invalid");
        }
        if (config.advanced_bank_prefix.size() != 2) {
            throw std::invalid_argument("advanced bank prefix must contain exactly 2 characters");
        }
        if (config.advanced_bank_enabled &&
            config.advanced_bank_prefix == config.data_bank_prefix) {
            throw std::invalid_argument("advanced and data bank prefixes must differ");
        }
    });

FrontendCollectorModeTimeGrouping::FrontendCollectorModeTimeGrouping(
    FrontendCollectorModeContext& context,
    FrontendCollectorModeTimeGroupingConfig config)
    : FrontendCollectorModeDefault(
          context,
          groupingConfig(config),
          config.advanced_bank_enabled,
          std::move(config.advanced_bank_prefix),
          "time_grouping") {}
