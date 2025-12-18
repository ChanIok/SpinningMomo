module;

module Features.Settings.Compute;

import std;
import Core.State;
import Core.I18n.Types;
import Core.I18n.State;
import Features.Settings.Menu;
import Features.Settings.Types;
import Features.Settings.State;
import Features.Settings.Registry;
import Utils.String;
import Utils.Logger;

namespace Features::Settings::Compute {

auto compute_presets_from_config(const Types::AppSettings& config,
                                 const Core::I18n::Types::TextData& texts)
    -> State::ComputedPresets {
  State::ComputedPresets computed;

  // 处理比例预设
  for (const auto& ratio_id : config.ui.app_menu.aspect_ratios) {
    if (auto ratio = Registry::parse_aspect_ratio(ratio_id)) {
      std::wstring name(ratio_id.begin(), ratio_id.end());
      computed.aspect_ratios.emplace_back(name, *ratio);
    } else {
      Logger().warn("Invalid aspect ratio in settings: '{}', skipping", ratio_id);
    }
  }

  // 处理分辨率预设
  for (const auto& resolution_id : config.ui.app_menu.resolutions) {
    if (auto resolution = Registry::parse_resolution(resolution_id)) {
      std::wstring name(resolution_id.begin(), resolution_id.end());
      auto [w, h] = *resolution;
      computed.resolutions.emplace_back(name, w, h);
    } else {
      Logger().warn("Invalid resolution in settings: '{}', skipping", resolution_id);
    }
  }

  return computed;
}

auto trigger_compute(Core::State::AppState& app_state) -> bool {
  app_state.settings->computed =
      compute_presets_from_config(app_state.settings->raw, app_state.i18n->texts);
  return true;
}

}  // namespace Features::Settings::Compute