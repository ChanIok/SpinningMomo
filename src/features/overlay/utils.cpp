module;

#include <windows.h>

#include <iostream>

module Features.Overlay.Utils;

import std;
import Core.State;
import Features.Overlay.State;
import Features.Overlay.Types;

namespace Features::Overlay::Utils {

auto get_screen_dimensions() -> std::pair<int, int> {
  return {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}

auto calculate_overlay_dimensions(int game_width, int game_height, int screen_width,
                                  int screen_height) -> std::pair<int, int> {
  double aspect_ratio = static_cast<double>(game_width) / game_height;

  int window_width, window_height;
  if (game_width * screen_height <= screen_width * game_height) {
    // 基于高度计算宽度
    window_height = screen_height;
    window_width = static_cast<int>(screen_height * aspect_ratio);
  } else {
    // 基于宽度计算高度
    window_width = screen_width;
    window_height = static_cast<int>(screen_width / aspect_ratio);
  }

  return {window_width, window_height};
}

auto should_use_overlay(int game_width, int game_height, int screen_width, int screen_height)
    -> bool {
  return game_width > screen_width || game_height > screen_height;
}

auto get_window_dimensions(HWND hwnd) -> std::expected<std::pair<int, int>, std::string> {
  RECT rect;
  if (!GetWindowRect(hwnd, &rect)) {
    DWORD error = GetLastError();
    return std::unexpected(std::format("Failed to get window rect. Error: {}", error));
  }

  return std::make_pair(rect.right - rect.left, rect.bottom - rect.top);
}

auto calculate_letterbox_area(int screen_width, int screen_height, int game_width, int game_height)
    -> std::tuple<int, int, int, int> {
  // 计算游戏内容的宽高比
  float game_aspect = static_cast<float>(game_width) / game_height;

  // 计算屏幕的宽高比
  float screen_aspect = static_cast<float>(screen_width) / screen_height;

  // 计算实际的游戏显示区域
  int content_left = 0, content_top = 0, content_width = 0, content_height = 0;

  if (game_aspect > screen_aspect) {
    // 游戏比例更宽 - 上下黑边 (letterbox)
    content_width = screen_width;
    content_height = static_cast<int>(screen_width / game_aspect);
    content_left = 0;
    content_top = (screen_height - content_height) / 2;
  } else if (game_aspect < screen_aspect) {
    // 游戏比例更窄 - 左右黑边 (pillarbox)
    content_width = static_cast<int>(screen_height * game_aspect);
    content_height = screen_height;
    content_left = (screen_width - content_width) / 2;
    content_top = 0;
  } else {
    // 完美匹配，无黑边
    content_width = screen_width;
    content_height = screen_height;
    content_left = 0;
    content_top = 0;
  }

  return std::make_tuple(content_left, content_top, content_width, content_height);
}

}  // namespace Features::Overlay::Utils
