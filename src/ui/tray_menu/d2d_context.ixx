module;

#include <d2d1.h>
#include <dwrite.h>
#include <windows.h>

export module UI.TrayMenu.D2DContext;

import std;
import Core.State;
import Types.UI;

namespace UI::TrayMenu::D2DContext {

// 初始化托盘菜单的Direct2D资源
export auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool;

// 清理托盘菜单的Direct2D资源
export auto cleanup_d2d(Core::State::AppState& state) -> void;

// 调整托盘菜单渲染目标大小
export auto resize_d2d(Core::State::AppState& state, const SIZE& new_size) -> bool;

}  // namespace UI::TrayMenu::D2DContext
