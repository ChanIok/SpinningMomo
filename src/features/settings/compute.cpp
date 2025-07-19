module;

module Features.Settings.Compute;

import std;
import Features.Settings.Types;
import Features.Settings.State;
import Types.Presets;
import Common.MenuData.Types;
import Core.Constants;
import Utils.String;

namespace Features::Settings::Compute {

// 简单的比例解析
auto parse_ratio(const std::string& id) -> std::optional<double> {
  // 预定义的比例映射
  static const std::map<std::string, double> predefined = {
      {"32:9", 32.0 / 9.0}, {"21:9", 21.0 / 9.0}, {"16:9", 16.0 / 9.0}, {"3:2", 3.0 / 2.0},
      {"1:1", 1.0},         {"3:4", 3.0 / 4.0},   {"2:3", 2.0 / 3.0},   {"9:16", 9.0 / 16.0}};

  if (predefined.contains(id)) {
    return predefined.at(id);
  }

  // 解析自定义格式 "W:H"
  auto pos = id.find(':');
  if (pos != std::string::npos) {
    try {
      double w = std::stod(id.substr(0, pos));
      double h = std::stod(id.substr(pos + 1));
      if (h > 0) {
        return w / h;
      }
    } catch (...) {
      // 解析失败，返回nullopt
    }
  }

  return std::nullopt;
}

// 简单的分辨率解析
auto parse_resolution(const std::string& id) -> std::optional<std::pair<int, int>> {
  // 预定义的分辨率映射
  static const std::map<std::string, std::pair<int, int>> predefined = {
      {"Default", {0, 0}},    {"1080P", {1920, 1080}}, {"2K", {2560, 1440}},
      {"4K", {3840, 2160}},   {"6K", {5760, 3240}},    {"8K", {7680, 4320}},
      {"12K", {11520, 6480}}, {"480P", {720, 480}},    {"720P", {1280, 720}},
      {"5K", {5120, 2880}},   {"10K", {10240, 4320}},  {"16K", {15360, 8640}}};

  if (predefined.contains(id)) {
    return predefined.at(id);
  }

  // 解析自定义格式 "WxH"
  auto pos = id.find('x');
  if (pos != std::string::npos) {
    try {
      int w = std::stoi(id.substr(0, pos));
      int h = std::stoi(id.substr(pos + 1));
      if (w > 0 && h > 0) {
        return std::make_pair(w, h);
      }
    } catch (...) {
      // 解析失败，返回nullopt
    }
  }

  return std::nullopt;
}

// 计算功能项预设
auto compute_feature_items_from_config(const Types::AppSettings& config)
    -> std::vector<Common::MenuData::Types::ComputedFeatureItem> {
  std::vector<Common::MenuData::Types::ComputedFeatureItem> computed_items;

  // 处理功能项，过滤启用的项目并按顺序排序
  std::vector<std::pair<Types::FeatureItem, int>> enabled_items;
  for (const auto& item : config.app_menu.feature_items) {
    if (item.enabled) {
      enabled_items.emplace_back(item, item.order);
    }
  }

  // 按 order 排序
  std::sort(enabled_items.begin(), enabled_items.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

  // 转换为 ComputedFeatureItem
  for (const auto& [item, order] : enabled_items) {
    std::wstring text = Utils::String::FromUtf8(item.label);
    computed_items.emplace_back(text, item.id, item.enabled, item.order);
  }

  return computed_items;
}

auto compute_presets_from_config(const Types::AppSettings& config) -> State::ComputedPresets {
  State::ComputedPresets computed;

  // 处理比例预设
  for (const auto& item : config.app_menu.aspect_ratios) {
    if (item.enabled) {
      if (auto ratio = parse_ratio(item.id)) {
        std::wstring name(item.id.begin(), item.id.end());
        computed.aspect_ratios.emplace_back(name, *ratio);
      }
    }
  }

  // 处理分辨率预设
  for (const auto& item : config.app_menu.resolutions) {
    if (item.enabled) {
      if (auto resolution = parse_resolution(item.id)) {
        std::wstring name(item.id.begin(), item.id.end());
        auto [w, h] = *resolution;
        computed.resolutions.emplace_back(name, w, h);
      }
    }
  }

  // 处理功能项预设
  computed.feature_items = compute_feature_items_from_config(config);

  return computed;
}

auto update_computed_state(State::SettingsState& state) -> bool {
  state.computed_presets = compute_presets_from_config(state.config);
  return true;
}

}  // namespace Features::Settings::Compute