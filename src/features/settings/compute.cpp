#include "features/settings/compute.hpp"

#include "vendor/std.hpp"

#include "core/i18n/state.hpp"
#include "core/i18n/types.hpp"
#include "core/state/app_state.hpp"
#include "features/settings/menu.hpp"
#include "features/settings/registry.hpp"
#include "features/settings/state.hpp"
#include "features/settings/types.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace features::settings::compute {

auto compute_presets_from_config(const AppSettings& config, const core::i18n::TextData& texts)
    -> ComputedPresets {
  ComputedPresets computed;

  // 处理比例预设
  for (const auto& ratio_id : config.ui.app_menu.aspect_ratios) {
    if (auto ratio = registry::parse_aspect_ratio(ratio_id)) {
      std::wstring name(ratio_id.begin(), ratio_id.end());
      computed.aspect_ratios.emplace_back(name, *ratio);
    } else {
      Logger().warn("Invalid aspect ratio in settings: '{}', skipping", ratio_id);
    }
  }

  // 处理分辨率预设
  for (const auto& resolution_id : config.ui.app_menu.resolutions) {
    if (auto resolution = registry::parse_resolution(resolution_id)) {
      std::wstring name(resolution_id.begin(), resolution_id.end());
      auto [w, h] = *resolution;
      computed.resolutions.emplace_back(name, w, h);
    } else {
      Logger().warn("Invalid resolution in settings: '{}', skipping", resolution_id);
    }
  }

  return computed;
}

auto trigger_compute(core::AppState& app_state) -> bool {
  app_state.settings->computed =
      compute_presets_from_config(app_state.settings->raw, app_state.i18n->texts);
  return true;
}

}  // namespace features::settings::compute
