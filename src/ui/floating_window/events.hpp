#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace ui::floating_window::events {

// 比例改变事件
struct RatioChangeEvent {
  size_t index;
  std::wstring ratio_name;
  double ratio_value;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 分辨率改变事件
struct ResolutionChangeEvent {
  size_t index;
  std::wstring resolution_name;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 功能开关事件
struct PreviewToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct OverlayToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct LetterboxToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct RecordingToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 窗口动作事件
struct CaptureEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct ScreenshotsEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct HideEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct ExitEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct ToggleVisibilityEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

struct WindowSelectionEvent {
  std::wstring window_title;
  HWND window_handle;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

}  // namespace ui::floating_window::events
