module;

export module Utils.Display;

import std;
import <windows.h>;

namespace Utils::Display {

export struct MonitorInfo {
  RECT monitor_rect{};
  RECT work_rect{};
  bool primary = false;
};

export auto rect_width(const RECT& rect) -> int;
export auto rect_height(const RECT& rect) -> int;

export auto get_monitor_for_window(HWND hwnd) -> std::expected<MonitorInfo, std::string>;

}  // namespace Utils::Display
