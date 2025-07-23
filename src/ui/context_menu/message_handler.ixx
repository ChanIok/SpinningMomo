module;

#include <windows.h>

export module UI.ContextMenu.MessageHandler;

import std;

namespace UI::ContextMenu::MessageHandler {

// 静态窗口过程函数，是模块与Windows消息系统交互的唯一入口
export auto CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    -> LRESULT;

}  // namespace UI::ContextMenu::MessageHandler