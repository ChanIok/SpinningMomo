module;

#include <windows.h>

module Utils.Display;

import std;

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

  return MonitorInfo{.monitor_rect = monitor_info.rcMonitor, .work_rect = monitor_info.rcWork};
}

auto get_monitor_for_window(HWND hwnd) -> std::expected<MonitorInfo, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Invalid target window for monitor lookup."};
  }

  return query_monitor_info(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

BOOL CALLBACK find_primary_monitor_proc(HMONITOR monitor, HDC /*hdc*/, LPRECT /*rect*/,
                                        LPARAM lparam) {
  MONITORINFO monitor_info{.cbSize = sizeof(MONITORINFO)};
  if (!GetMonitorInfoW(monitor, &monitor_info)) {
    return TRUE;
  }

  if ((monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0) {
    *reinterpret_cast<HMONITOR*>(lparam) = monitor;
    return FALSE;
  }

  return TRUE;
}

auto get_primary_monitor() -> std::expected<MonitorInfo, std::string> {
  HMONITOR primary_monitor = nullptr;
  if (!EnumDisplayMonitors(nullptr, nullptr, find_primary_monitor_proc,
                           reinterpret_cast<LPARAM>(&primary_monitor)) ||
      !primary_monitor) {
    return std::unexpected{"Failed to resolve primary display monitor."};
  }

  return query_monitor_info(primary_monitor);
}

auto get_working_monitor(HWND anchor_hwnd, bool anchor_visible)
    -> std::expected<MonitorInfo, std::string> {
  if (anchor_visible && anchor_hwnd && IsWindow(anchor_hwnd)) {
    return get_monitor_for_window(anchor_hwnd);
  }

  return get_primary_monitor();
}

}  // namespace Utils::Display
