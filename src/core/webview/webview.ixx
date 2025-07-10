module;

#include <windows.h>

export module Core.WebView;

import std;
import Core.State;
import Core.WebView.State;

namespace Core::WebView {

// 初始化函数
export auto initialize(Core::State::AppState& state, HWND parent_hwnd)
    -> std::expected<void, std::string>;

// 销毁函数
export auto shutdown(Core::State::AppState& state) -> void;

// 窗口操作
export auto show_webview(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto hide_webview(Core::State::AppState& state) -> void;
export auto resize_webview(Core::State::AppState& state, int width, int height) -> void;
export auto move_webview(Core::State::AppState& state, int x, int y) -> void;

// 导航操作
export auto navigate_to_url(Core::State::AppState& state, const std::wstring& url)
    -> std::expected<void, std::string>;
export auto navigate_to_string(Core::State::AppState& state, const std::wstring& html)
    -> std::expected<void, std::string>;

// 消息通信
export auto send_message(Core::State::AppState& state, const std::string& message)
    -> std::expected<std::string, std::string>;
export auto post_message(Core::State::AppState& state, const std::string& message) -> void;
export auto register_message_handler(Core::State::AppState& state, const std::string& message_type,
                                     std::function<void(const std::string&)> handler) -> void;

// 状态查询
export auto is_webview_ready(const Core::State::AppState& state) -> bool;
export auto is_webview_loading(const Core::State::AppState& state) -> bool;
export auto get_current_url(const Core::State::AppState& state) -> std::wstring;

// 开发工具
export auto open_dev_tools(Core::State::AppState& state) -> void;
export auto close_dev_tools(Core::State::AppState& state) -> void;

}  // namespace Core::WebView