#pragma once
#include "sampic_event_builder/model/event.h"
#include <span>
namespace sampic::event_builder {
class ProductBankDecoder {
public:
    Event decode(std::span<const std::uint8_t> bank) const;
};
}
