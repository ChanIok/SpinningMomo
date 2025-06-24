module;

#include <windows.h>

export module Features.Preview.Interaction;

import std;
import Types.Preview;
import Core.State;

export namespace Features::Preview::Interaction {

// 消息处理结果
enum class MessageResult {
  Handled,     // 消息已处理
  NotHandled,  // 消息未处理，继续默认处理
  Default      // 使用默认窗口过程
};

// 主消息处理函数
auto handle_preview_message(Core::State::AppState& state, HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) -> std::pair<MessageResult, LRESULT>;

// 具体的消息处理函数
auto handle_paint(Core::State::AppState& state, HWND hwnd) -> LRESULT;
auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_left_button_up(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_mouse_wheel(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_sizing(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_size(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam) -> LRESULT;
auto handle_dpi_changed(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_nc_hit_test(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;
auto handle_right_button_up(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT;

// 窗口拖拽相关
auto start_window_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void;
auto update_window_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void;
auto end_window_drag(Core::State::AppState& state, HWND hwnd) -> void;

// 视口拖拽相关
auto start_viewport_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void;
auto update_viewport_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void;
auto end_viewport_drag(Core::State::AppState& state, HWND hwnd) -> void;

// 窗口缩放相关
auto handle_window_scaling(Core::State::AppState& state, HWND hwnd, int wheel_delta,
                           POINT mouse_pos) -> void;

// 点击位置检测
auto is_point_in_title_bar(const Core::State::AppState& state, POINT pt) -> bool;
auto is_point_in_viewport(const Core::State::AppState& state, POINT pt) -> bool;
auto get_border_hit_test(const Core::State::AppState& state, HWND hwnd, POINT pt) -> LRESULT;

// 游戏窗口操作
auto move_game_window_to_position(Core::State::AppState& state, float relative_x, float relative_y)
    -> void;
auto center_game_window_if_possible(Core::State::AppState& state) -> void;

}  // namespace Features::Preview::Interaction