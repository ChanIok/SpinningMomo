#pragma once

#include "vendor/std.hpp"

namespace features::adb_mode::events {

// 通知悬浮窗和命令系统 ADB 设备是否已经连接或断开。
struct ConnectionChangedEvent {
  bool connected = false;
};

// 通知悬浮窗某次比例/分辨率变换的结果，并携带需要同步的菜单索引。
struct DisplayTransformCompletedEvent {
  std::optional<std::size_t> ratio_index;
  std::optional<std::size_t> resolution_index;
  bool success = false;
  std::string error;
};

}  // namespace features::adb_mode::events
