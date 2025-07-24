module;

export module Features.Settings.Events;

import std;
import Features.Settings.Types;

namespace Features::Settings::Events {

// 设置变更事件
export struct SettingsChangeEvent {
  Features::Settings::Types::SettingsChangeData data;
  
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

}  // namespace Features::Settings::Events