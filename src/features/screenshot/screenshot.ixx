module;

#include <windows.h>

export module Features.Screenshot;

import std;
import Features.Screenshot.State;
import Utils.Graphics.Capture;

namespace Features::Screenshot {

// 主要API：异步截图
export auto take_screenshot(
    Features::Screenshot::State::ScreenshotState& state, HWND target_window,
    std::function<void(bool success, const std::wstring& path)> completion_callback = nullptr)
    -> std::expected<void, std::string>;

// 工具函数：检查截图功能是否支持
export auto is_supported() -> bool;

// 系统管理函数
export auto cleanup_system(Features::Screenshot::State::ScreenshotState& state) -> void;

}  // namespace Features::Screenshot