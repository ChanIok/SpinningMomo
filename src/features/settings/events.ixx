module;

export module Core.Settings.Events;

import std;
import Features.Settings.Types;

namespace Core::Settings::Events {

// 设置变更事件
export struct SettingsChangeEvent {
  Features::Settings::Types::SettingsChangeData data;
  
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

}  // namespace Core::Settings::Events