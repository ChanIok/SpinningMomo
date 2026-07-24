#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "core/webview/state.hpp"
#include "core/webview/static.hpp"
#include "core/webview/types.hpp"

namespace core::webview {

// 初始化函数
auto initialize(core::AppState& state, HWND webview_hwnd) -> std::expected<void, std::string>;

// 检测本机 WebView2 Runtime 版本
auto get_runtime_version() -> std::expected<std::string, std::string>;

// 销毁函数
auto shutdown(core::AppState& state) -> void;

// 窗口操作
auto resize_webview(core::AppState& state, int width, int height) -> void;

// 导航操作
auto navigate_to_url(core::AppState& state, const std::wstring& url)
    -> std::expected<void, std::string>;

// 消息通信
auto send_message(core::AppState& state, const std::string& message)
    -> std::expected<std::string, std::string>;
auto post_message(core::AppState& state, const std::string& message) -> void;
auto register_message_handler(core::AppState& state, const std::string& message_type,
                              std::move_only_function<void(const std::string&) const> handler)
    -> void;
auto register_document_created_script(core::AppState& state, std::string script_id,
                                      std::wstring script_source) -> void;
auto register_virtual_host_folder_mapping(core::AppState& state, std::wstring host_name,
                                          std::wstring folder_path,
                                          core::webview::VirtualHostResourceAccessKind access_kind)
    -> void;
auto unregister_virtual_host_folder_mapping(core::AppState& state, std::wstring_view host_name)
    -> void;
// 请求协调虚拟主机映射：通过 PostMessage 触发窗口线程执行 reconcile
auto request_virtual_host_folder_mapping_reconcile(core::AppState& state) -> void;
// 执行虚拟主机映射协调：在 WebView 所在线程中比对期望状态与已应用状态，差量更新
auto reconcile_virtual_host_folder_mappings(core::AppState& state) -> void;
auto apply_background_mode_from_settings(core::AppState& state) -> void;
auto get_loading_background_color(core::AppState& state) -> COLORREF;
auto is_composition_active(core::AppState& state) -> bool;

// Composition hosting 输入转发
auto forward_mouse_message(core::AppState& state, HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    -> bool;
auto forward_non_client_right_button_message(core::AppState& state, HWND hwnd, UINT msg,
                                             WPARAM wparam, LPARAM lparam) -> bool;
auto hit_test_non_client_region(core::AppState& state, HWND hwnd, LPARAM lparam)
    -> std::optional<LRESULT>;

}  // namespace core::webview
