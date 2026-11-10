#include "utils/display/display.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "utils/string/string.hpp"

namespace utils::display {

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

auto list_monitors() -> std::expected<std::vector<MonitorDevice>, std::string> {
  UINT32 path_count = 0;
  UINT32 mode_count = 0;
  std::vector<DISPLAYCONFIG_PATH_INFO> paths;
  std::vector<DISPLAYCONFIG_MODE_INFO> modes;
  LONG result;
  // 显示配置可能在两次查询之间变化，缓冲区不足时重新获取大小。
  do {
    result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count);
    if (result != ERROR_SUCCESS) {
      return std::unexpected{std::format("Failed to query display configuration size: {}", result)};
    }
    paths.resize(path_count);
    modes.resize(mode_count);
    result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count,
                                modes.data(), nullptr);
  } while (result == ERROR_INSUFFICIENT_BUFFER);
  if (result != ERROR_SUCCESS) {
    return std::unexpected{std::format("Failed to query display configuration: {}", result)};
  }
  paths.resize(path_count);

  struct TargetInfo {
    std::wstring gdi_name;
    std::string device_path;
    std::string name;
  };
  std::vector<TargetInfo> targets;
  for (const auto& path : paths) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
    source.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source),
                     path.sourceInfo.adapterId, path.sourceInfo.id};
    result = DisplayConfigGetDeviceInfo(&source.header);
    if (result != ERROR_SUCCESS) {
      return std::unexpected{std::format("Failed to query display source name: {}", result)};
    }
    DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
    target.header = {DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME, sizeof(target),
                     path.targetInfo.adapterId, path.targetInfo.id};
    result = DisplayConfigGetDeviceInfo(&target.header);
    if (result != ERROR_SUCCESS) {
      return std::unexpected{std::format("Failed to query display target name: {}", result)};
    }
    targets.push_back({source.viewGdiDeviceName, utils::string::ToUtf8(target.monitorDevicePath),
                       utils::string::ToUtf8(target.monitorFriendlyDeviceName)});
  }

  struct Enumeration {
    const std::vector<TargetInfo>& targets;
    std::vector<MonitorDevice> monitors;
    std::string error;
  } enumeration{targets, {}, {}};
  const auto callback = [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
    auto& enumeration = *reinterpret_cast<Enumeration*>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info))) {
      enumeration.error = std::format("Failed to query display monitor: {}", GetLastError());
      return FALSE;
    }
    // 镜像模式下多个物理输出共享同一个桌面区域，只提供一个可放置窗口的屏幕选项。
    const auto target = std::ranges::find_if(enumeration.targets, [&](const auto& target) {
      return _wcsicmp(target.gdi_name.c_str(), info.szDevice) == 0;
    });
    if (target != enumeration.targets.end() && !target->device_path.empty()) {
      enumeration.monitors.push_back({target->device_path,
                                      target->name,
                                      (info.dwFlags & MONITORINFOF_PRIMARY) != 0,
                                      {info.rcMonitor, info.rcWork}});
    }
    return TRUE;
  };
  if (!EnumDisplayMonitors(nullptr, nullptr, callback, reinterpret_cast<LPARAM>(&enumeration))) {
    return std::unexpected{enumeration.error.empty() ? "Failed to enumerate display monitors."
                                                     : enumeration.error};
  }
  std::ranges::sort(enumeration.monitors, [](const auto& lhs, const auto& rhs) {
    if (lhs.primary != rhs.primary) {
      return lhs.primary;
    }
    return lhs.device_path < rhs.device_path;
  });
  return std::move(enumeration.monitors);
}

auto get_monitor_by_device_path(std::string_view device_path)
    -> std::expected<MonitorInfo, std::string> {
  auto monitors = list_monitors();
  if (!monitors) {
    return std::unexpected{monitors.error()};
  }
  const auto selected_path = utils::string::FromUtf8(std::string(device_path));
  for (const auto& monitor : *monitors) {
    if (_wcsicmp(utils::string::FromUtf8(monitor.device_path).c_str(), selected_path.c_str()) ==
        0) {
      return monitor.info;
    }
  }
  return std::unexpected{"The selected display is not connected. Please select another display."};
}

auto get_monitor_for_window(HWND hwnd) -> std::expected<MonitorInfo, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Invalid target window for monitor lookup."};
  }

  return query_monitor_info(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

auto get_primary_monitor() -> std::expected<MonitorInfo, std::string> {
  // Windows 主显示器包含虚拟桌面原点。
  const auto monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  if (!monitor) {
    return std::unexpected{"Failed to resolve primary display monitor."};
  }

  return query_monitor_info(monitor);
}

auto get_working_monitor(HWND anchor_hwnd, bool anchor_visible)
    -> std::expected<MonitorInfo, std::string> {
  if (anchor_visible && anchor_hwnd && IsWindow(anchor_hwnd)) {
    return get_monitor_for_window(anchor_hwnd);
  }

  return get_primary_monitor();
}

}  // namespace utils::display
