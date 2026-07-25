#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace ui::notification_window::message_handler {

LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

}  // namespace ui::notification_window::message_handler
