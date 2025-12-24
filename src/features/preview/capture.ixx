module;

export module Features.Preview.Capture;

import std;
import Core.State;
import <windows.h>;

export namespace Features::Preview::Capture {

// 初始化捕获系统
auto initialize_capture(Core::State::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string>;

// 开始捕获
auto start_capture(Core::State::AppState& state) -> std::expected<void, std::string>;

// 停止捕获
auto stop_capture(Core::State::AppState& state) -> void;

// 清理捕获资源
auto cleanup_capture(Core::State::AppState& state) -> void;

}  // namespace Features::Preview::Capture