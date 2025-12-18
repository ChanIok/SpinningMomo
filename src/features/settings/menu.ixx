module;

export module Features.Settings.Menu;

import std;

namespace Features::Settings::State {
export struct SettingsState;
}

namespace Features::Settings::Menu {

// === Types (Merged from menu_data/types.ixx) ===

// 比例预设
export struct RatioPreset {
  std::wstring name;  // 比例名称
  double ratio;       // 宽高比值

  constexpr RatioPreset(const std::wstring& n, double r) noexcept : name(n), ratio(r) {}
};

// 分辨率预设
export struct ResolutionPreset {
  std::wstring name;           // 显示名称（如 "4K"）
  std::uint64_t total_pixels;  // 总像素数
  int base_width;              // 基准宽度
  int base_height;             // 基准高度

  constexpr ResolutionPreset(const std::wstring& n, int w, int h) noexcept
      : name(n), total_pixels(static_cast<std::uint64_t>(w) * h), base_width(w), base_height(h) {}
};

// === Getters Interface ===

// 获取当前的比例预设数据
export auto get_ratios(const Features::Settings::State::SettingsState& state)
    -> const std::vector<RatioPreset>&;

// 获取当前的分辨率预设数据
export auto get_resolutions(const Features::Settings::State::SettingsState& state)
    -> const std::vector<ResolutionPreset>&;

}  // namespace Features::Settings::Menu
