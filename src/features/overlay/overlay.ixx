module;

#include <windows.h>

export module Features.Overlay;

import std;
import Core.State;
import Features.Overlay.State;

namespace Features::Overlay {

// 初始化叠加层系统
export auto initialize_overlay(Core::State::AppState& state, HINSTANCE instance, HWND parent)
    -> std::expected<void, std::string>;

// 开始叠加层捕获
export auto start_overlay(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string>;

// 停止叠加层
export auto stop_overlay(Core::State::AppState& state) -> void;

// 设置黑边模式
export auto set_letterbox_mode(Core::State::AppState& state, bool enabled) -> void;

// 清理资源
export auto cleanup_overlay(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay
