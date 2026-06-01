module;

#include <windows.h>

export module UI.NotificationWindow.MessageHandler;

namespace UI::NotificationWindow::MessageHandler {

export LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

}  // namespace UI::NotificationWindow::MessageHandler
