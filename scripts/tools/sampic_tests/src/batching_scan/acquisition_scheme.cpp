#include "sampic_tests/batching_scan/acquisition_scheme.h"

#include <stdexcept>

namespace sampic::batching_scan {

AcquisitionScheme acquisition_scheme_from_string(std::string_view value) {
  if (value == "l2_external_gate") {
    return AcquisitionScheme::L2ExternalGate;
  }
  if (value == "self_trigger") {
    return AcquisitionScheme::SelfTrigger;
  }
  if (value == "external") {
    return AcquisitionScheme::External;
  }
  throw std::runtime_error(
      "Unknown acquisition scheme '" + std::string(value) +
      "'; expected 'l2_external_gate', 'self_trigger', or 'external'");
}

std::string acquisition_scheme_name(AcquisitionScheme scheme) {
  switch (scheme) {
    case AcquisitionScheme::L2ExternalGate:
      return "l2_external_gate";
    case AcquisitionScheme::SelfTrigger:
      return "self_trigger";
    case AcquisitionScheme::External:
      return "external";
  }
  throw std::runtime_error("Invalid acquisition scheme");
}

}  // namespace sampic::batching_scan
