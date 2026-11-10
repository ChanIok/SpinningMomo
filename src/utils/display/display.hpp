#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace utils::display {

struct MonitorInfo {
  RECT monitor_rect{};
  RECT work_rect{};
};

struct MonitorDevice {
  std::string device_path;
  std::string name;
  bool primary = false;
  MonitorInfo info;
};

// 当前参与桌面显示的屏幕，主屏优先；设备路径用于保存选择，不能保存句柄或列表序号。
auto list_monitors() -> std::expected<std::vector<MonitorDevice>, std::string>;

auto get_monitor_by_device_path(std::string_view device_path)
    -> std::expected<MonitorInfo, std::string>;

auto get_primary_monitor() -> std::expected<MonitorInfo, std::string>;

auto rect_width(const RECT& rect) -> int;
auto rect_height(const RECT& rect) -> int;

// anchor_visible 为 true 且 anchor_hwnd 为有效窗口时，查该窗口最近显示器；否则主屏
auto get_working_monitor(HWND anchor_hwnd, bool anchor_visible)
    -> std::expected<MonitorInfo, std::string>;

}  // namespace utils::display
