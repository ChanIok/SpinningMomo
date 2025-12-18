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

// 将菜单ID映射到本地化文本
auto get_localized_text_for_menu_id(const std::string& menu_id,
                                    const Core::I18n::Types::TextData& texts) -> std::wstring {
  // 直接使用字符串比较，映射到新的i18n结构
  if (menu_id == "screenshot.capture") {
    return Utils::String::FromUtf8(texts.menu.screenshot_capture);
  } else if (menu_id == "screenshot.open_folder") {
    return Utils::String::FromUtf8(texts.menu.screenshot_open_folder);
  } else if (menu_id == "feature.toggle_preview") {
    return Utils::String::FromUtf8(texts.menu.preview_toggle);
  } else if (menu_id == "feature.toggle_overlay") {
    return Utils::String::FromUtf8(texts.menu.overlay_toggle);
  } else if (menu_id == "feature.toggle_letterbox") {
    return Utils::String::FromUtf8(texts.menu.letterbox_toggle);
  } else if (menu_id == "feature.toggle_recording") {
    return Utils::String::FromUtf8(texts.menu.recording_toggle);
  } else if (menu_id == "window.reset_transform") {
    return Utils::String::FromUtf8(texts.menu.window_reset);
  } else if (menu_id == "panel.hide") {
    return Utils::String::FromUtf8(texts.menu.app_hide);
  } else if (menu_id == "app.exit") {
    return Utils::String::FromUtf8(texts.menu.app_exit);
  } else {
    // 如果ID不匹配，返回ID本身作为fallback
    return Utils::String::FromUtf8(menu_id);
  }
}


// 计算功能项预设
auto compute_feature_items_from_config(const Types::AppSettings& config,
                                       const Core::I18n::Types::TextData& texts)
    -> std::vector<Features::Settings::Menu::ComputedFeatureItem> {
  std::vector<Features::Settings::Menu::ComputedFeatureItem> computed_items;

  // 遍历启用列表，验证并转换为 ComputedFeatureItem
  for (size_t index = 0; index < config.ui.app_menu.enabled_features.size(); ++index) {
    const auto& feature_id = config.ui.app_menu.enabled_features[index];

    // 验证功能 ID 是否合法
    if (!Registry::is_valid_feature(feature_id)) {
      Logger().warn("Invalid feature ID in settings: '{}', skipping", feature_id);
      continue;
    }

    // 获取本地化文本
    std::wstring text = get_localized_text_for_menu_id(feature_id, texts);
    computed_items.emplace_back(text, feature_id, true, static_cast<int>(index));
  }

  return computed_items;
}

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

  // 处理功能项预设
  computed.feature_items = compute_feature_items_from_config(config, texts);

  return computed;
}

auto trigger_compute(Core::State::AppState& app_state) -> bool {
  app_state.settings->computed =
      compute_presets_from_config(app_state.settings->raw, app_state.i18n->texts);
  return true;
}

}  // namespace Features::Settings::Compute