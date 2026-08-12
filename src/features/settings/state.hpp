#pragma once

#include "vendor/std.hpp"

#include "features/settings/menu_types.hpp"
#include "features/settings/types.hpp"

namespace features::settings {

// 计算后的预设状态
struct ComputedPresets {
  std::vector<features::settings::menu::RatioPreset> aspect_ratios;
  std::vector<features::settings::menu::ResolutionPreset> resolutions;
};

// Settings 模块的完整运行时状态 (Vue/Pinia Style)
struct SettingsState {
  // 设置可能由多个 RPC 工作线程同时更新；const 运行时查询也需要加锁读取。
  mutable std::mutex mutation_mutex;

  // [Raw State] 原始配置数据 (Source of Truth)
  AppSettings raw;

  // [Computed State] 计算后的缓存 (Derived State)
  ComputedPresets computed;

  bool is_initialized = false;
};

}  // namespace features::settings
