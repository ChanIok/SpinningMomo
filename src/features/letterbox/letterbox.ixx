module;

#include <windows.h>

export module Features.Letterbox;

import std;
import Core.State;

namespace Features::Letterbox {

// 初始化和清理
export auto initialize(Core::State::AppState& state, HINSTANCE instance)
    -> std::expected<void, std::string>;

export auto shutdown(Core::State::AppState& state) -> std::expected<void, std::string>;

// 窗口操作
export auto show(Core::State::AppState& state, HWND target_window = nullptr)
    -> std::expected<void, std::string>;

export auto hide(Core::State::AppState& state) -> std::expected<void, std::string>;

}  // namespace Features::Letterbox