module;

export module Features.Settings.State;

import std;
import Features.Settings.Types;
import Features.Settings.Menu.Types;

namespace Features::Settings::State {

// 计算后的预设状态
export struct ComputedPresets {
  std::vector<Features::Settings::Menu::RatioPreset> aspect_ratios;
  std::vector<Features::Settings::Menu::ResolutionPreset> resolutions;
};

// Settings 模块的完整运行时状态 (Vue/Pinia Style)
export struct SettingsState {
  // 设置可能由多个 RPC 工作线程同时更新；一次合并、计算和落盘必须作为完整事务执行。
  std::mutex mutation_mutex;

  // [Raw State] 原始配置数据 (Source of Truth)
  Types::AppSettings raw;

  // [Computed State] 计算后的缓存 (Derived State)
  ComputedPresets computed;

  bool is_initialized = false;
};

}  // namespace Features::Settings::State
