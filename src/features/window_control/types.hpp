#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace features::window_control {

struct WindowInfo {
  HWND handle = nullptr;
  std::wstring title;

  auto operator==(const WindowInfo& other) const noexcept -> bool {
    return handle == other.handle && title == other.title;
  }
};

struct Resolution {
  int width;
  int height;

  auto operator==(const Resolution& other) const noexcept -> bool {
    return width == other.width && height == other.height;
  }
};

struct TransformOptions {
  bool activate_window = true;
  std::optional<HWND> letterbox_window = std::nullopt;
};

struct ResolutionPresetInput {
  int base_width = 0;
  int base_height = 0;
};

struct ResolutionCalculationOptions {
  bool align_to_8 = false;
  bool use_short_edge = false;
  int screen_width = 0;
  int screen_height = 0;
};

}  // namespace features::window_control
