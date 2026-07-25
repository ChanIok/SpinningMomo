#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace ui::floating_window {

// 窗口创建和销毁
auto create_window(core::AppState& state) -> std::expected<void, std::string>;
auto destroy_window(core::AppState& state) -> void;

// 窗口显示控制
auto show_window(core::AppState& state) -> void;
auto hide_window(core::AppState& state) -> void;
auto toggle_visibility(core::AppState& state) -> void;

// 更新UI状态
auto set_current_ratio(core::AppState& state, size_t index) -> void;
auto set_current_resolution(core::AppState& state, size_t index) -> void;

// 更新菜单项
auto update_menu_items(core::AppState& state) -> void;

// 渲染触发
auto request_repaint(core::AppState& state) -> void;

// 刷新 DWM 实际绘制的窗口边框宽度
auto refresh_visible_frame_border_thickness(core::AppState& state) -> void;

// 设置变更响应
auto refresh_from_settings(core::AppState& state) -> void;

// 注册窗口类
auto register_window_class(HINSTANCE instance) -> void;

// 初始化菜单项
auto initialize_menu_items(core::AppState& state) -> void;

// 创建窗口样式和属性
auto create_window_attributes(HWND hwnd) -> void;

}  // namespace ui::floating_window
