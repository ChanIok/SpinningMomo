module;

export module UI.WebViewWindow;

import std;
import Core.State;
import Vendor.Windows;

namespace UI::WebViewWindow {

// 窗口初始化和清理
export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto cleanup(Core::State::AppState& state) -> void;

// 窗口显示控制
export auto toggle_visibility(Core::State::AppState& state) -> void;

// 窗口控制功能
export auto minimize_window(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto maximize_window(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto restore_window(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto close_window(Core::State::AppState& state) -> std::expected<void, std::string>;

// 窗口拖拽功能
export auto start_drag_window(Core::State::AppState& state, int screen_x, int screen_y) 
    -> std::expected<void, std::string>;
export auto move_drag_window(Core::State::AppState& state, int screen_x, int screen_y) 
    -> std::expected<void, std::string>;
export auto end_drag_window(Core::State::AppState& state) 
    -> std::expected<void, std::string>;

}  // namespace UI::WebViewWindow