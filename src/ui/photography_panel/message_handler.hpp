#pragma once

#include <windows.h>

namespace UI::PhotographyPanel::MessageHandler {

LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

}  // namespace UI::PhotographyPanel::MessageHandler
