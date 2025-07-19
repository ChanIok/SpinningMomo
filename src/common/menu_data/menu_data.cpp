module;

module Common.MenuData;

import std;
import Core.State;
import Types.Presets;

namespace Common::MenuData {

auto get_current_aspect_ratios(const Core::State::AppState& state)
    -> const std::vector<Types::Presets::RatioPreset>& {
  return state.settings.computed_presets.aspect_ratios;
}

auto get_current_resolutions(const Core::State::AppState& state)
    -> const std::vector<Types::Presets::ResolutionPreset>& {
  return state.settings.computed_presets.resolutions;
}

}  // namespace Common::MenuData