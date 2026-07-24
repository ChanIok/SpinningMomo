#include "features/settings/menu.hpp"

#include "core/state/app_state.hpp"
#include "features/settings/state.hpp"

namespace Features::Settings::Menu {

auto get_ratios(const Core::State::AppState& app_state) -> const std::vector<RatioPreset>& {
  static const std::vector<RatioPreset> kEmpty;
  if (!app_state.settings) {
    return kEmpty;
  }
  const auto& settings = *app_state.settings;
  return settings.computed.aspect_ratios;
}

auto get_resolutions(const Core::State::AppState& app_state)
    -> const std::vector<ResolutionPreset>& {
  static const std::vector<ResolutionPreset> kEmpty;
  if (!app_state.settings) {
    return kEmpty;
  }
  const auto& settings = *app_state.settings;
  return settings.computed.resolutions;
}

}  // namespace Features::Settings::Menu
