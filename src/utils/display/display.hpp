#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace utils::display {

struct MonitorInfo {
  RECT monitor_rect{};
  RECT work_rect{};
};

auto rect_width(const RECT& rect) -> int;
auto rect_height(const RECT& rect) -> int;

// anchor_visible 为 true 且 anchor_hwnd 为有效窗口时，查该窗口最近显示器；否则主屏
auto get_working_monitor(HWND anchor_hwnd, bool anchor_visible)
    -> std::expected<MonitorInfo, std::string>;

}  // namespace utils::display
