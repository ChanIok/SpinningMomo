#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace ui::webview_window {

// 窗口初始化和清理
auto initialize(core::AppState& state) -> std::expected<void, std::string>;
auto recreate_webview_host(core::AppState& state) -> std::expected<void, std::string>;
auto cleanup(core::AppState& state) -> void;

// 窗口显示控制；route 为空时打开默认首页，非空时打开对应前端 hash 路由。
// temporary_size 仅影响当前窗口会话，不覆盖持久化的主窗口尺寸。
auto activate_window(core::AppState& state, std::wstring_view route = {},
                     std::optional<SIZE> temporary_size = std::nullopt) -> void;

// 窗口控制功能
auto minimize_window(core::AppState& state) -> std::expected<void, std::string>;
auto toggle_maximize_window(core::AppState& state) -> std::expected<void, std::string>;
auto set_fullscreen_window(core::AppState& state, bool fullscreen)
    -> std::expected<void, std::string>;
auto close_window(core::AppState& state) -> std::expected<void, std::string>;

}  // namespace ui::webview_window
