module;

export module Core.WebView;

import std;
import Core.State;
import Core.WebView.State;
import <windows.h>;

namespace Core::WebView {

// 初始化函数
export auto initialize(Core::State::AppState& state, HWND webview_hwnd)
    -> std::expected<void, std::string>;

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
                                     
}  // namespace Core::WebView