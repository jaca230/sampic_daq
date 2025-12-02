#include "sampic_tests/common/time_utils.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace sampic::common {

std::string TimeUtils::Iso8601Now() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  const auto fractional = std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds);
  std::time_t tt = clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.'
      << std::setfill('0') << std::setw(3) << fractional.count() << "Z";
  return oss.str();
}

}  // namespace sampic::common
