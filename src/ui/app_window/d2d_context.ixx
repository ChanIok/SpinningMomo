module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module UI.AppWindow.D2DContext;

import std;
import Core.State;
import Types.UI;

namespace UI::AppWindow::D2DContext {

// 初始化Direct2D资源
export auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool;

// 清理Direct2D资源
export auto cleanup_d2d(Core::State::AppState& state) -> void;

// 调整渲染目标大小
export auto resize_d2d(Core::State::AppState& state, const SIZE& new_size) -> bool;

// 更新文本格式（DPI变化时）
export auto update_text_format_if_needed(Core::State::AppState& state) -> bool;

}  // namespace UI::AppWindow::D2DContext
