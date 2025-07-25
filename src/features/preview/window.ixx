module;

#include <windows.h>

export module Features.Preview.Window;

import std;
import Features.Preview.State;
import Core.State;

namespace Features::Preview::Window {

// 初始化预览系统
export auto initialize_preview(Core::State::AppState& state, HINSTANCE instance, HWND parent)
    -> std::expected<void, std::string>;

// 开始捕获并显示预览
export auto start_preview(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string>;

// 停止预览并隐藏窗口
export auto stop_preview(Core::State::AppState& state) -> void;

// DPI 处理
export auto update_preview_dpi(Core::State::AppState& state, UINT new_dpi) -> void;

// 清理资源
export auto cleanup_preview(Core::State::AppState& state) -> void;

}  // namespace Features::Preview::Window