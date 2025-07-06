module;

#include <windows.h>

export module Features.Letterbox.State;

import std;

export namespace Features::Letterbox::State {

struct LetterboxState {
  HWND window_handle{nullptr};
  HWND target_window{nullptr};
  HWND message_window{nullptr};
  HINSTANCE instance{nullptr};
  bool is_visible{false};
  bool is_initialized{false};

  // 线程和事件相关
  std::jthread event_thread;
  HWINEVENTHOOK event_hook{nullptr};
  DWORD target_process_id{0};
};

struct LetterboxConfig {
  bool auto_hide_on_minimize{true};
  bool auto_show_on_foreground{true};
  bool manage_taskbar_order{true};
  std::chrono::milliseconds taskbar_delay{10};
};

// 自定义消息定义
constexpr UINT WM_TARGET_WINDOW_FOREGROUND = WM_USER + 100;
constexpr UINT WM_HIDE_LETTERBOX = WM_USER + 101;
constexpr UINT WM_SHOW_LETTERBOX = WM_USER + 102;
constexpr UINT WM_UPDATE_TASKBAR_ZORDER = WM_USER + 103;

// 计时器ID定义
constexpr UINT TIMER_TASKBAR_ZORDER = 1001;

}  // namespace Features::Letterbox::State