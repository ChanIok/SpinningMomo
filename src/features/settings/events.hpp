#pragma once

#include "features/settings/types.hpp"

namespace Features::Settings::Events {

// 设置变更事件
struct SettingsChangeEvent {
  Features::Settings::Types::SettingsChangeData data;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

}  // namespace Features::Settings::Events