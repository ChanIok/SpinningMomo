module;

export module UI.FloatingWindow.MessageHandler;

import <windows.h>;

namespace UI::FloatingWindow::MessageHandler {

export LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

}  // namespace UI::FloatingWindow::MessageHandler
