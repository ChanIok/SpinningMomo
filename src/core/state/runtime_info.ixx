module;

export module Core.State.RuntimeInfo;

import std;

namespace Core::State::RuntimeInfo {

// 应用基本信息结构
export struct RuntimeInfoState {
  // 版本信息
  std::string version;
  unsigned int major_version = 0;
  unsigned int minor_version = 0;
  unsigned int patch_version = 0;
  unsigned int build_number = 0;

  // 系统信息
  std::string os_name;
  unsigned int os_major_version = 0;
  unsigned int os_minor_version = 0;
  unsigned int os_build_number = 0;

  // WebView2 运行时信息
  bool is_webview2_available = false;
  std::string webview2_version;
  bool is_debug_build = false;

  // Windows Graphics Capture 支持状态 (Windows 10 1903 build 18362 or later)
  bool is_capture_supported = false;
  bool is_cursor_capture_control_supported = false;  // IsCursorCaptureEnabled
  bool is_border_control_supported = false;          // IsBorderRequired (黄色边框)

  // Process Loopback 支持状态 (Windows 10 2004 build 19041 or later)
  bool is_process_loopback_audio_supported = false;
};

}  // namespace Core::State::RuntimeInfo
