module;

#include <windows.h>

export module UI.TrayMenu.MessageHandler;

import std;
import Core.State;

namespace UI::TrayMenu::MessageHandler {

// 静态窗口过程函数
export auto CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    -> LRESULT;

// 回滚版本：恢复必要的消息处理函数
auto handle_paint(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_key_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_kill_focus(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_size(Core::State::AppState& state, HWND hwnd) -> LRESULT;
export auto handle_timer(Core::State::AppState& state, HWND hwnd, WPARAM timer_id) -> LRESULT;

}  // namespace UI::TrayMenu::MessageHandler