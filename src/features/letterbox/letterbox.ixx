module;

#include <windows.h>

export module Features.Letterbox;

import std;
import Core.State;
import Features.Letterbox.State;

namespace Features::Letterbox {

// 初始化和清理
export auto initialize(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string>;

export auto shutdown(Core::State::AppState& state) -> std::expected<void, std::string>;

// 窗口操作
export auto show(Core::State::AppState& state, HWND target_window = nullptr)
    -> std::expected<void, std::string>;

export auto hide(Core::State::AppState& state) -> std::expected<void, std::string>;

export auto update_position(Core::State::AppState& state, HWND target_window = nullptr)
    -> std::expected<void, std::string>;

// 状态查询
export auto is_visible(const Core::State::AppState& state) -> bool;
export auto is_event_thread_running(const Core::State::AppState& state) -> bool;

// 事件管理
export auto start_event_monitoring(Core::State::AppState& state,
                                   const State::LetterboxConfig& config = {})
    -> std::expected<void, std::string>;

export auto stop_event_monitoring(Core::State::AppState& state) -> std::expected<void, std::string>;

}  // namespace Features::Letterbox