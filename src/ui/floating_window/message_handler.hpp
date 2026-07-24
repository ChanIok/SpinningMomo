#pragma once

#include <windows.h>

namespace UI::FloatingWindow::MessageHandler {

LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

}  // namespace UI::FloatingWindow::MessageHandler
