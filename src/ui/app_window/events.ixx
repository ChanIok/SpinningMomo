module;

export module UI.AppWindow.Events;

import std;
import Vendor.Windows;

namespace UI::AppWindow::Events {

// DPI改变事件
export struct DpiChangeEvent {
  Vendor::Windows::UINT new_dpi;
  Vendor::Windows::SIZE window_size;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 比例改变事件
export struct RatioChangeEvent {
  size_t index;
  std::wstring ratio_name;
  double ratio_value;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 分辨率改变事件
export struct ResolutionChangeEvent {
  size_t index;
  std::wstring resolution_name;
  std::uint64_t total_pixels;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 功能开关事件
export struct PreviewToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct OverlayToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct LetterboxToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct RecordingToggleEvent {
  bool enabled;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

// 窗口动作事件
export struct CaptureEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct ScreenshotsEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct HideEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct ExitEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct WebViewEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct ToggleVisibilityEvent {
  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

export struct WindowSelectionEvent {
  std::wstring window_title;
  Vendor::Windows::HWND window_handle;

  std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

}  // namespace UI::AppWindow::Events