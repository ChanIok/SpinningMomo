#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

namespace ui::floating_window::message_handler {

LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

}  // namespace ui::floating_window::message_handler
