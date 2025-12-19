module;

export module Features.Settings.Registry;

import std;

namespace Features::Settings::Registry {

// === 比例预设注册表 ===
// 内置的比例预设映射（用于快速查找，但用户可以添加任意 W:H 格式）
export inline const std::map<std::string_view, double> ASPECT_RATIO_PRESETS = {
    {"32:9", 32.0 / 9.0}, {"21:9", 21.0 / 9.0}, {"16:9", 16.0 / 9.0}, {"3:2", 3.0 / 2.0},
    {"1:1", 1.0},         {"3:4", 3.0 / 4.0},   {"2:3", 2.0 / 3.0},   {"9:16", 9.0 / 16.0},
};

// === 分辨率预设注册表 ===
// 内置的分辨率别名（用户也可以使用 WxH 格式）
export inline const std::map<std::string_view, std::pair<int, int>> RESOLUTION_ALIASES = {
    {"Default", {0, 0}},  {"480P", {720, 480}},   {"720P", {1280, 720}},  {"1080P", {1920, 1080}},
    {"2K", {2560, 1440}}, {"4K", {3840, 2160}},   {"5K", {5120, 2880}},   {"6K", {5760, 3240}},
    {"8K", {7680, 4320}}, {"10K", {10240, 4320}}, {"12K", {11520, 6480}}, {"16K", {15360, 8640}},
};

// === 解析函数 ===

// 解析比例 ID (如 "16:9" 或 "4:3")
export auto parse_aspect_ratio(std::string_view id) -> std::optional<double> {
  // 先查预设
  if (ASPECT_RATIO_PRESETS.contains(id)) {
    return ASPECT_RATIO_PRESETS.at(id);
  }

  // 解析自定义格式 "W:H"
  std::string id_str{id};
  auto pos = id_str.find(':');
  if (pos != std::string::npos) {
    try {
      double w = std::stod(id_str.substr(0, pos));
      double h = std::stod(id_str.substr(pos + 1));
      if (h > 0) {
        return w / h;
      }
    } catch (...) {
      // 解析失败
    }
  }

  return std::nullopt;
}

// 解析分辨率 ID (如 "4K" 或 "1920x1080")
export auto parse_resolution(std::string_view id) -> std::optional<std::pair<int, int>> {
  // 先查别名
  if (RESOLUTION_ALIASES.contains(id)) {
    return RESOLUTION_ALIASES.at(id);
  }

  // 解析自定义格式 "WxH"
  std::string id_str{id};
  auto pos = id_str.find('x');
  if (pos != std::string::npos) {
    try {
      int w = std::stoi(id_str.substr(0, pos));
      int h = std::stoi(id_str.substr(pos + 1));
      if (w > 0 && h > 0) {
        return std::make_pair(w, h);
      }
    } catch (...) {
      // 解析失败
    }
  }

  return std::nullopt;
}

}  // namespace Features::Settings::Registry
