#pragma once
#include "sampic_event_builder/model/event.h"
#include <span>
namespace sampic::event_builder {
class ProductBankEncoder {
public:
    std::vector<std::uint8_t> encode(const Event& event) const;
};
}
