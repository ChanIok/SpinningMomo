module;

export module Utils.Display;

import std;
import <windows.h>;

namespace Utils::Display {

export struct MonitorInfo {
  RECT monitor_rect{};
};

export auto rect_width(const RECT& rect) -> int;
export auto rect_height(const RECT& rect) -> int;

// anchor_visible 为 true 且 anchor_hwnd 为有效窗口时，查该窗口最近显示器；否则主屏
export auto get_working_monitor(HWND anchor_hwnd, bool anchor_visible)
    -> std::expected<MonitorInfo, std::string>;

}  // namespace Utils::Display
