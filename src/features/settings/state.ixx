module;

export module Features.Settings.State;

import std;
import Features.Settings.Types;
import Common.MenuData.Types;

export namespace Features::Settings::State {

// 计算后的预设状态
struct ComputedPresets {
  std::vector<Common::MenuData::Types::RatioPreset> aspect_ratios;
  std::vector<Common::MenuData::Types::ResolutionPreset> resolutions;
  std::vector<Common::MenuData::Types::ComputedFeatureItem> feature_items;
};

// Settings 模块的完整运行时状态
struct SettingsState {
  Types::AppSettings config;         // 配置数据（可序列化）
  ComputedPresets computed_presets;  // 计算状态
  bool is_initialized = false;
};

// === 状态管理函数 ===

// 创建默认的设置状态
inline auto create_default_settings_state() -> SettingsState {
  SettingsState state;
  state.config = Types::AppSettings{};
  state.is_initialized = false;
  return state;
}

}  // namespace Features::Settings::State