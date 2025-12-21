module;

#include <windows.h>

export module UI.FloatingWindow.MessageHandler;

import Core.State;
import UI.FloatingWindow.State;

namespace UI::FloatingWindow::MessageHandler {

export LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

}  // namespace UI::FloatingWindow::MessageHandler