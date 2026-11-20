#pragma once

#include <string_view>

namespace sampic::event_builder {

class Stage {
public:
    virtual ~Stage() = default;
    [[nodiscard]] virtual std::string_view type() const = 0;
};

} // namespace sampic::event_builder
