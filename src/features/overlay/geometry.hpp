#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace features::overlay::geometry {

// 计算窗口尺寸
auto calculate_overlay_dimensions(int game_width, int game_height, int screen_width,
                                  int screen_height) -> std::pair<int, int>;

// 检查游戏窗口是否需要叠加层
auto should_use_overlay(int game_width, int game_height, int screen_width, int screen_height)
    -> bool;

// 获取游戏窗口尺寸
auto get_window_dimensions(HWND hwnd) -> std::expected<std::pair<int, int>, std::string>;

// 计算黑边区域的位置和尺寸
auto calculate_letterbox_area(int screen_width, int screen_height, int game_width, int game_height)
    -> std::tuple<int, int, int, int>;

}  // namespace features::overlay::geometry
