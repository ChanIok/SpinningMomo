module;

export module UI.WebViewWindow;

import std;
import Core.State;
import Vendor.Windows;

namespace UI::WebViewWindow {

// 窗口创建和销毁
export auto create(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto destroy(Core::State::AppState& state) -> void;

// 窗口显示控制
export auto show(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto hide(Core::State::AppState& state) -> void;
export auto toggle_visibility(Core::State::AppState& state) -> void;

// 窗口属性获取
export auto get_hwnd(const Core::State::AppState& state) -> Vendor::Windows::HWND;
export auto is_visible(const Core::State::AppState& state) -> bool;

// 内部函数
auto register_window_class(Vendor::Windows::HINSTANCE instance) -> void; 
auto window_proc(Vendor::Windows::HWND hwnd, Vendor::Windows::UINT msg, 
                Vendor::Windows::WPARAM wparam, Vendor::Windows::LPARAM lparam) -> Vendor::Windows::LRESULT;

}  // namespace UI::WebViewWindow 