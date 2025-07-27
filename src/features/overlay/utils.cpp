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

}  // namespace Features::Overlay::Utils
