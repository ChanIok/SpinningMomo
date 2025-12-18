module;

module Features.Settings.Menu;

import std;
import Features.Settings.State;

namespace Features::Settings::Menu {

auto get_ratios(const Features::Settings::State::SettingsState& state)
    -> const std::vector<RatioPreset>& {
  return state.computed.aspect_ratios;
}

auto get_resolutions(const Features::Settings::State::SettingsState& state)
    -> const std::vector<ResolutionPreset>& {
  return state.computed.resolutions;
}

}  // namespace Features::Settings::Menu
