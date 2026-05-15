module;

module Utils.Display;

import std;
import <windows.h>;

namespace Utils::Display {

auto rect_width(const RECT& rect) -> int { return rect.right - rect.left; }

auto rect_height(const RECT& rect) -> int { return rect.bottom - rect.top; }

auto query_monitor_info(HMONITOR monitor) -> std::expected<MonitorInfo, std::string> {
  if (!monitor) {
    return std::unexpected{"Invalid monitor handle."};
  }

  MONITORINFO monitor_info{.cbSize = sizeof(MONITORINFO)};
  if (!GetMonitorInfoW(monitor, &monitor_info)) {
    return std::unexpected{
        std::format("Failed to query monitor info (GetMonitorInfoW), error: {}", GetLastError())};
  }

  return MonitorInfo{
      .monitor_rect = monitor_info.rcMonitor,
      .work_rect = monitor_info.rcWork,
      .primary = (monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0,
  };
}

auto get_monitor_for_window(HWND hwnd) -> std::expected<MonitorInfo, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Invalid target window for monitor lookup."};
  }

  return query_monitor_info(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

}  // namespace Utils::Display
