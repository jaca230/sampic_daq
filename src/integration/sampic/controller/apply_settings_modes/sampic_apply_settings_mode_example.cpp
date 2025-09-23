#include "integration/sampic/controller/apply_settings_modes/sampic_apply_settings_mode_example.h"
#include "integration/sampic/config/sampic_crate_configurator.h"
#include "integration/sampic/config/sampic_board_configurator.h"
#include "integration/sampic/config/sampic_chip_configurator.h"
#include "integration/sampic/config/sampic_channel_configurator.h"
#include <spdlog/spdlog.h>

void SampicApplySettingsModeExample::apply() {
    spdlog::info("ApplySettingsModeExample: Setting trigger options...");

    try {
        // --- Crate-level ---
        SampicCrateConfigurator crateCfg(info_, params_, settings_);
        crateCfg.setExternalTriggerType();

        // --- Loop over boards / chips / channels ---
        for (auto& [boardKey, boardCfg] : settings_.front_end_boards) {
            int boardIdx = crateCfg.indexFromKey(boardKey);
            SampicBoardConfigurator boardConfigurator(boardIdx, info_, params_, boardCfg);

            for (auto& [chipKey, chipCfg] : boardCfg.sampics) {
                int chipIdx = boardConfigurator.indexFromKey(chipKey);
                SampicChipConfigurator chipConfigurator(boardIdx, chipIdx, info_, params_, chipCfg);

                // SAMPIC_TRIGGER_IS_L1
                chipConfigurator.setTriggerOption();

                for (auto& [chKey, chCfg] : chipCfg.channels) {
                    int chIdx = chipConfigurator.indexFromKey(chKey);
                    SampicChannelConfigurator chConfigurator(boardIdx, chipIdx, chIdx,
                                                             info_, params_, chCfg);

                    // Enable channel + set EXT_TRIGGER_MODE
                    chConfigurator.setMode();
                    chConfigurator.setTriggerMode();
                }
            }
        }

        spdlog::info("ApplySettingsModeExample: Trigger options set successfully.");

    } catch (const std::exception& e) {
        spdlog::error("ApplySettingsModeExample: Exception during apply: {}", e.what());
        throw;
    }
}
