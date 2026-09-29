#pragma once

#include <string>
#include <string_view>

namespace sampic::batching_scan {

enum class AcquisitionScheme {
  L2ExternalGate,
  SelfTrigger,
  External,
};

AcquisitionScheme acquisition_scheme_from_string(std::string_view value);
std::string acquisition_scheme_name(AcquisitionScheme scheme);

}  // namespace sampic::batching_scan
