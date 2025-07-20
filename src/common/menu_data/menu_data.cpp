module;

module Common.MenuData;

import std;
import Core.State;
import Features.Settings.State;
import Common.MenuData.Types;

namespace Common::MenuData {

auto get_current_aspect_ratios(const Core::State::AppState& state)
    -> const std::vector<Common::MenuData::Types::RatioPreset>& {
  return state.settings->computed_presets.aspect_ratios;
}

auto get_current_resolutions(const Core::State::AppState& state)
    -> const std::vector<Common::MenuData::Types::ResolutionPreset>& {
  return state.settings->computed_presets.resolutions;
}

auto get_current_feature_items(const Core::State::AppState& state)
    -> const std::vector<Common::MenuData::Types::ComputedFeatureItem>& {
  return state.settings->computed_presets.feature_items;
}

}  // namespace Common::MenuData