module;

export module Core.WebView;

import std;
import Core.State;
import Core.WebView.State;
import Core.WebView.Static;
import Core.WebView.Types;
import <windows.h>;

namespace Core::WebView {

// 初始化函数
export auto initialize(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string>;

// 检测本机 WebView2 Runtime 版本
export auto get_runtime_version() -> std::expected<std::string, std::string>;

// 销毁函数
export auto shutdown(Core::State::AppState& state) -> void;

// 窗口操作
export auto resize_webview(Core::State::AppState& state, int width, int height) -> void;

// 导航操作
export auto navigate_to_url(Core::State::AppState& state, const std::wstring& url)
    -> std::expected<void, std::string>;

// 消息通信
export auto send_message(Core::State::AppState& state, const std::string& message)
    -> std::expected<std::string, std::string>;
export auto post_message(Core::State::AppState& state, const std::string& message) -> void;
export auto register_message_handler(Core::State::AppState& state, const std::string& message_type,
                                     std::function<void(const std::string&)> handler) -> void;
export auto register_virtual_host_folder_mapping(
    Core::State::AppState& state, std::wstring host_name, std::wstring folder_path,
    Core::WebView::State::VirtualHostResourceAccessKind access_kind) -> void;
export auto unregister_virtual_host_folder_mapping(Core::State::AppState& state,
                                                   std::wstring_view host_name) -> void;
export auto apply_background_mode_from_settings(Core::State::AppState& state) -> void;
export auto get_loading_background_color(Core::State::AppState& state) -> COLORREF;
export auto is_composition_active(Core::State::AppState& state) -> bool;

// Composition hosting 输入转发
export auto forward_mouse_message(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam) -> bool;
export auto forward_non_client_right_button_message(Core::State::AppState& state, HWND hwnd,
                                                    UINT msg, WPARAM wparam, LPARAM lparam) -> bool;
export auto hit_test_non_client_region(Core::State::AppState& state, HWND hwnd, LPARAM lparam)
    -> std::optional<LRESULT>;

}  // namespace Core::WebView
