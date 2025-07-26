module;

#include <windows.h>

export module Features.Overlay.Utils;

import std;
import Features.Overlay.State;
import Core.State;

namespace Features::Overlay::Utils {

// 获取屏幕尺寸
export auto get_screen_dimensions() -> std::pair<int, int>;

// 计算窗口尺寸
export auto calculate_overlay_dimensions(int game_width, int game_height, int screen_width,
                                         int screen_height) -> std::pair<int, int>;

// 检查游戏窗口是否需要叠加层
export auto should_use_overlay(int game_width, int game_height, int screen_width, int screen_height)
    -> bool;

// 获取游戏窗口尺寸
export auto get_window_dimensions(HWND hwnd) -> std::expected<std::pair<int, int>, std::string>;

// 获取游戏窗口矩形
export auto get_window_rect_safe(HWND hwnd) -> std::expected<RECT, std::string>;

}  // namespace Features::Overlay::Utils
