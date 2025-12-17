module;

export module Features.Settings.State;

import std;
import Features.Settings.Types;
import Features.Settings.Menu;

namespace Features::Settings::State {

// 计算后的预设状态
export struct ComputedPresets {
  std::vector<Features::Settings::Menu::RatioPreset> aspect_ratios;
  std::vector<Features::Settings::Menu::ResolutionPreset> resolutions;
  std::vector<Features::Settings::Menu::ComputedFeatureItem> feature_items;
};

// Settings 模块的完整运行时状态 (Vue/Pinia Style)
export struct SettingsState {
  // [Raw State] 原始配置数据 (Source of Truth)
  Types::AppSettings raw;

  // [Computed State] 计算后的缓存 (Derived State)
  ComputedPresets computed;

  bool is_initialized = false;
};

// === 状态管理函数 ===

// 创建默认的设置状态
export auto create_default_settings_state() -> SettingsState {
  SettingsState state;
  state.raw = Types::AppSettings{};
  state.is_initialized = false;
  return state;
}

}  // namespace Features::Settings::State
