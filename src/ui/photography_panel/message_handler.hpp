#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace ui::photography_panel::message_handler {

LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

}  // namespace ui::photography_panel::message_handler
