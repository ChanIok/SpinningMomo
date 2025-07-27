module;

#include <windows.h>

export module Features.Screenshot;

import std;
import Core.State;
import Features.Screenshot.State;
import Utils.Graphics.Capture;

namespace Features::Screenshot {

// 主要API：异步截图
export auto take_screenshot(
    Core::State::AppState& state, HWND target_window,
    std::function<void(bool success, const std::wstring& path)> completion_callback = nullptr)
    -> std::expected<void, std::string>;

// 系统管理函数
export auto cleanup_system(Core::State::AppState& state) -> void;

}  // namespace Features::Screenshot