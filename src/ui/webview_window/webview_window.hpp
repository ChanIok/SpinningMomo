#pragma once

#include "core/state/app_state.hpp"
#include "vendor/windows.hpp"

namespace UI::WebViewWindow {

// 窗口初始化和清理
auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;
auto recreate_webview_host(Core::State::AppState& state) -> std::expected<void, std::string>;
auto cleanup(Core::State::AppState& state) -> void;

// 窗口显示控制；route 为空时打开默认首页，非空时打开对应前端 hash 路由。
auto activate_window(Core::State::AppState& state, std::wstring_view route = {}) -> void;

// 窗口控制功能
auto minimize_window(Core::State::AppState& state) -> std::expected<void, std::string>;
auto toggle_maximize_window(Core::State::AppState& state) -> std::expected<void, std::string>;
auto set_fullscreen_window(Core::State::AppState& state, bool fullscreen)
    -> std::expected<void, std::string>;
auto close_window(Core::State::AppState& state) -> std::expected<void, std::string>;

}  // namespace UI::WebViewWindow
