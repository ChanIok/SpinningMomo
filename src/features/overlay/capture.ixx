module;

export module Features.Overlay.Capture;

import std;
import Core.State;
import Vendor.Windows;

namespace Features::Overlay::Capture {

// 初始化捕获系统
export auto initialize_capture(Core::State::AppState& state, Vendor::Windows::HWND target_window,
                               int width, int height) -> std::expected<void, std::string>;

// 开始捕获
export auto start_capture(Core::State::AppState& state) -> std::expected<void, std::string>;

// 停止捕获
export auto stop_capture(Core::State::AppState& state) -> void;

// 清理捕获资源
export auto cleanup_capture(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay::Capture
