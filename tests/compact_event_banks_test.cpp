#include "processing/sampic_processing/collector/banks/frontend_event_hit_banks.h"
#include "processing/sampic_processing/collector/modes/default/default_config.h"
#include "processing/sampic_processing/collector/modes/time_grouping/time_grouping_config.h"
#include "processing/sampic_processing/collector/modes/external_trigger/external_trigger_config.h"
#include "processing/sampic_processing/collector/modes/vendor_passthrough/vendor_passthrough_config.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T read(const FrontendEventBank& bank, std::size_t offset = 0) {
    T value{}; std::memcpy(&value, bank.data() + offset, sizeof(T)); return value;
}
template<class Config> void checkSwitches(const std::vector<const HitStruct*>& hits) {
    Config cfg;
    check(!cfg.verbose_banks, "verbose defaults");
    validateFrontendHitBanks(cfg, {});
    FrontendEvent compact;
    addFrontendHitBanks(compact, {}, hits, cfg);
    check(compact.banks().size() == 2, "compact defaults");
    check(compact.findBankByPrefix("SD") && compact.findBankByPrefix("SH"), "default bank prefixes");
    cfg.verbose_banks = true;
    FrontendEvent verbose;
    addFrontendHitBanks(verbose, {}, hits, cfg);
    check(verbose.banks().size() == 2, "verbose-only selection");
    check(dynamic_cast<FrontendEventBankDataVerbose*>(verbose.findBankByPrefix("SD")) != nullptr, "toggle selects verbose SD");
    check(dynamic_cast<FrontendEventBankAdvancedHitsVerbose*>(verbose.findBankByPrefix("SH")) != nullptr, "toggle selects verbose SH");
    check(dynamic_cast<FrontendEventBankData*>(compact.findBankByPrefix("SD")) != nullptr, "toggle selects compact SD");
    cfg.advanced_bank_enabled = false;
    FrontendEvent data_only;
    addFrontendHitBanks(data_only, {}, hits, cfg);
    check(data_only.banks().size() == 1, "verbose toggle respects bank enables");
    cfg.advanced_bank_enabled = true;

    check(verbose.findBankByPrefix("SD") && verbose.findBankByPrefix("SH"), "verbose bank prefixes");
    cfg.data_bank_enabled = cfg.advanced_bank_enabled = true;
    cfg.advanced_bank_prefix = "SD";
    bool rejected = false;
    try { validateFrontendHitBanks(cfg, {}); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "duplicate prefixes must fail validation");
    cfg.advanced_bank_prefix = "SH";
    rejected = false;
    try { validateFrontendHitBanks(cfg, {{true, "SD"}}); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "metadata collision must fail validation");
    cfg.data_bank_enabled = cfg.advanced_bank_enabled = false;
    FrontendEvent empty;
    addFrontendHitBanks(empty, {}, hits, cfg);
    check(empty.banks().empty(), "disabled banks are not constructed");
}
int main(int argc, char** argv) {
    try {
        HitStruct hit{};
        hit.FeBoardIndex = 3; hit.Channel = 63; hit.HitNumber = 123;
        hit.SampicIndex = 3; hit.ChannelIndex = 15; hit.DataSize = 2;
        hit.ADCCorrected = TRUE; hit.INLCorrected = TRUE;
        hit.CellInfo = 42; hit.FirstCellPhysicalIndex = 17;
        hit.CorrectedDataSamples[0] = 0.1234f; hit.CorrectedDataSamples[1] = -0.0456f;
        hit.RawTOTValue = 60000; hit.TOTValue = 12.5f; hit.FirstCellTimeStamp = 123456789.125;
        hit.Amplitude = 0.4f; hit.Baseline = 0.2f; hit.Peak = 0.6f;
        hit.TimeIndex = 3; hit.TimeInstant = 5; hit.TimeAmplitude = 0.7f;
        hit.AdvancedParams.SampicDataHeader = 0x1234;
        hit.AdvancedParams.FirstTriggerPositionCell = 3;
        hit.AdvancedParams.TriggerPositionCell = 4;
        hit.AdvancedParams.ADCCounter_LatchedAtEndOfConv = 9;
        hit.AdvancedParams.StartOfADCRamp = 17;
        hit.AdvancedParams.TimePhysicalIndex = 42;
        hit.AdvancedParams.PhysicalCell0TimeStamp = 123456780.5;
        hit.AdvancedParams.SampicTimeStampA = 123; hit.AdvancedParams.SampicTimeStampB = 456;
        hit.AdvancedParams.FPGATimeStamp = 0x123456789abcdef0ULL;
        hit.AdvancedParams.TriggerPosition[0] = TRUE; hit.AdvancedParams.TriggerPosition[63] = TRUE;
        std::vector<const HitStruct*> hits{&hit, nullptr};
        FrontendEventBankData sd(hits);
        FrontendEventBankAdvancedHits sh(hits);
        auto header = read<FrontendEventBankData::Header>(sd);
        check(std::memcmp(header.magic, "SDC1", 4) == 0 && header.hit_count == 1 && header.record_size == 154, "SD header");
        auto record = read<FrontendEventBankData::Record>(sd, sizeof(header));
        check(record.waveform[0] == 1234 && record.waveform[1] == -456 && record.waveform[2] == 0, "ADC volt conversion/trailing zero");
        check(record.channel == 63 && record.feb_index == 3 && record.hit_number == 123, "identity fields");
        check(record.first_cell_timestamp_ns == hit.FirstCellTimeStamp && record.raw_tot == 60000 && record.calibrated_tot_ns == 12.5f, "scalar fields");
        check(record.flags == 3, "correction flags");
        auto sh_header = read<FrontendEventBankAdvancedHits::Header>(sh);
        auto timing = read<FrontendEventBankAdvancedHits::Record>(sh, sizeof(sh_header));
        check(sh_header.format_version == 2 && sh_header.record_size == 32 && sh_header.hit_count == 1, "SH header");
        check(timing.trigger_position_mask == 0x8000000000000001ULL && timing.fpga_timestamp == hit.AdvancedParams.FPGATimeStamp, "64-bit timing/mask");
        HitStruct second = hit;
        second.ADCCorrected = FALSE; second.DataSize = 1;
        second.CorrectedDataSamples[0] = 2047;
        FrontendEventBankData mixed({&hit, nullptr, &second});
        auto second_record = read<FrontendEventBankData::Record>(mixed, sizeof(header) + sizeof(record));
        check(read<FrontendEventBankData::Header>(mixed).hit_count == 2 &&
              second_record.waveform[0] == 2047 && !(second_record.flags & 2), "mixed ADC units and multi-hit stride");
        FrontendEventBankDataVerbose vd({}, hits);
        FrontendEventBankAdvancedHitsVerbose vh(hits);
        std::vector<std::uint8_t> legacy(vd.size()); vd.writeTo(legacy.data());
        const auto prefix = offsetof(HitStruct, RawDataSamples);
        const auto corrected = offsetof(HitStruct, CorrectedDataSamples);
        const auto suffix = offsetof(HitStruct, AdvancedParams);
        check(legacy.size() == 344 && std::memcmp(legacy.data(), &hit, prefix) == 0 &&
              std::memcmp(legacy.data() + prefix, reinterpret_cast<const char*>(&hit) + corrected, suffix-corrected) == 0, "legacy SD byte-for-byte preservation");
        check(vh.size() == 8+54 && read<FrontendEventBankAdvancedHitsVerbose::Header>(vh).format_version == 1, "legacy SH preservation");
        if (argc == 2) {
            std::filesystem::create_directories(argv[1]);
            for (auto* bank : std::vector<FrontendEventBank*>{&sd, &sh, &vd, &vh}) {
                std::vector<std::uint8_t> bytes(bank->size()); bank->writeTo(bytes.data());
                std::ofstream file(std::filesystem::path(argv[1]) / (std::string(bank->bankPrefix()) + ".bin"), std::ios::binary);
                file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            }
        }
        checkSwitches<FrontendCollectorModeDefaultConfig>(hits);
        checkSwitches<FrontendCollectorModeTimeGroupingConfig>(hits);
        checkSwitches<FrontendCollectorModeExternalTriggerConfig>(hits);
        checkSwitches<FrontendCollectorModeVendorPassthroughConfig>(hits);
        hit.ADCCorrected = FALSE; hit.INLCorrected = TRUE;
        hit.CorrectedDataSamples[0] = 2047.4f; hit.CorrectedDataSamples[1] = -100.6f;
        FrontendEventBankData adc_counts(hits);
        record = read<FrontendEventBankData::Record>(adc_counts, sizeof(header));
        check(record.waveform[0] == 2047 && record.waveform[1] == -101, "ADC counts without volt scaling");
        hit.DataSize = 4;
        hit.CorrectedDataSamples[0] = 1e30f; hit.CorrectedDataSamples[1] = -1e30f;
        hit.CorrectedDataSamples[2] = std::numeric_limits<float>::quiet_NaN();
        hit.CorrectedDataSamples[3] = std::numeric_limits<float>::infinity(); hit.RawTOTValue = -1;
        FrontendEventBankData clipped(hits);
        record = read<FrontendEventBankData::Record>(clipped, sizeof(header));
        check(record.waveform[0] == 32767 && record.waveform[1] == -32768 && record.waveform[2] == 0, "defined clipping/nonfinite handling");
        check((record.flags & 56) == 56 && record.raw_tot == 0, "conversion diagnostics");
        hit.DataSize = 65; bool rejected = false;
        try { FrontendEventBankData invalid(hits); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "invalid sample count");
        FrontendEventBankData empty({nullptr}); FrontendEventBankAdvancedHits empty_sh({nullptr});
        check(empty.size() == 12 && empty_sh.size() == 8, "zero-hit bank headers");
        std::weak_ptr<SampicEvent> weak;
        {
            auto parent = std::make_shared<SampicEvent>(); weak = parent;
            FrontendEventBankDataVerbose held({parent}, {}); parent.reset();
            check(!weak.expired(), "verbose zero-copy owner retained");
        }
        check(weak.expired(), "verbose owner released");
        std::cout << "Compact/verbose bank wire formats and all mode switches passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
