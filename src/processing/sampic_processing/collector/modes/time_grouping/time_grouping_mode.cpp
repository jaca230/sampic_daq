#include "processing/sampic_processing/collector/banks/frontend_event_hit_banks.h"
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
        config.advanced_bank_enabled,
        config.advanced_bank_prefix,
        config.verbose_banks,
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
        validateFrontendHitBanks(config, {
            {config.event_timing_bank_enabled, config.event_timing_bank_prefix},
            {config.collector_timing_bank_enabled, config.collector_timing_bank_prefix}});
        if (config.time_window_ns < 0 || config.finalize_after_ms < 0 ||
            config.wait_timeout_ms == 0) {
            throw std::invalid_argument("timing values are invalid");
        }
    });

FrontendCollectorModeTimeGrouping::FrontendCollectorModeTimeGrouping(
    FrontendCollectorModeContext& context,
    FrontendCollectorModeTimeGroupingConfig config)
    : FrontendCollectorModeDefault(
          context,
          groupingConfig(config),
          "time_grouping") {}
