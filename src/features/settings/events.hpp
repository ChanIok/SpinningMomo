#pragma once

#include "vendor/std.hpp"

#include "features/settings/types.hpp"

namespace features::settings::events {

// 设置变更事件
struct SettingsChangeEvent {
  features::settings::SettingsChangeData data;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

}  // namespace features::settings::events
