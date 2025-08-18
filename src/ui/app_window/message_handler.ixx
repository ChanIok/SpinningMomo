module;

#include <windows.h>

export module UI.AppWindow.MessageHandler;

import Core.State;
import UI.AppWindow.State;

namespace UI::AppWindow::MessageHandler {

export LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

}  // namespace UI::AppWindow::MessageHandler