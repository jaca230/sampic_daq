#ifndef FRONTEND_ODB_PATHS_H
#define FRONTEND_ODB_PATHS_H

#include <string>
#include <string_view>

namespace frontend::odb {

inline constexpr auto kSettingsBaseFormat = "/Equipment/SAMPIC %02d/Settings";

enum class Section {
  Logger,
  Frontend,
  Crate,
  SampicController,
  SampicEventCollector,
  FrontendEventCollector,
};

constexpr std::string_view suffix(Section section) {
  switch (section) {
    case Section::Logger: return "/Logger";
    case Section::Frontend: return "/Frontend";
    case Section::Crate: return "/Crate";
    case Section::SampicController: return "/Sampic Controller";
    case Section::SampicEventCollector: return "/Sampic Event Collector";
    case Section::FrontendEventCollector: return "/Frontend Event Collector";
  }
  return {};
}

inline std::string make_path(std::string_view base, Section section) {
  const auto suff = suffix(section);
  return std::string(base) + std::string(suff);
}

}  // namespace frontend::odb

#endif  // FRONTEND_ODB_PATHS_H

