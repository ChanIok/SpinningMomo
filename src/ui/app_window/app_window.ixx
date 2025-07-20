module;

export module UI.AppWindow;

import std;
import Core.State;
import UI.AppWindow.Layout;
import Vendor.Windows;

namespace UI::AppWindow {

// 窗口创建和销毁
export auto create_window(Core::State::AppState& state) -> std::expected<void, std::string>;
export auto destroy_window(Core::State::AppState& state) -> void;

// 窗口显示控制
export auto show_window(Core::State::AppState& state) -> void;
export auto hide_window(Core::State::AppState& state) -> void;
export auto toggle_visibility(Core::State::AppState& state) -> void;

// 更新UI状态
export auto set_current_ratio(Core::State::AppState& state, size_t index) -> void;
export auto set_current_resolution(Core::State::AppState& state, size_t index) -> void;
export auto set_preview_enabled(Core::State::AppState& state, bool enabled) -> void;
export auto set_overlay_enabled(Core::State::AppState& state, bool enabled) -> void;
export auto set_letterbox_enabled(Core::State::AppState& state, bool enabled) -> void;

// 更新菜单项
export auto update_menu_items(Core::State::AppState& state) -> void;
export auto set_menu_items_to_show(Core::State::AppState& state,
                                   std::span<const std::wstring> items) -> void;

// 热键处理
export auto register_hotkey(Core::State::AppState& state, Vendor::Windows::UINT modifiers,
                            Vendor::Windows::UINT key) -> bool;
export auto unregister_hotkey(Core::State::AppState& state) -> void;

// 渲染触发
export auto request_repaint(Core::State::AppState& state) -> void;

// 设置变更响应
export auto refresh_from_settings(Core::State::AppState& state) -> void;

// 注册窗口类
auto register_window_class(Vendor::Windows::HINSTANCE instance) -> void;

// 初始化菜单项
auto initialize_menu_items(Core::State::AppState& state) -> void;

// 创建窗口样式和属性
auto create_window_attributes(Vendor::Windows::HWND hwnd) -> void;

}  // namespace UI::AppWindow