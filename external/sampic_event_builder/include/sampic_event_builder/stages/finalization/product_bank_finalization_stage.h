#pragma once
#include "sampic_event_builder/serialization/product_bank_encoder.h"
#include "sampic_event_builder/stages/finalization/finalization_stage.h"
#include <string>
#include <utility>

namespace sampic::event_builder {
struct ProductBankFinalizationConfig { std::string bank_prefix{"SE"}; };
class ProductBankFinalizationStage final : public FinalizationStage {
public:
    explicit ProductBankFinalizationStage(ProductBankFinalizationConfig config = {}) : config_(std::move(config)) {}
    FinalizedEvent finalize(const PipelineEvent& event) const override;
    std::string_view type() const override { return "product-bank"; }
    const std::string& bank_prefix() const { return config_.bank_prefix; }
private: ProductBankFinalizationConfig config_;
};
} // namespace sampic::event_builder
