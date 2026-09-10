#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "ui/notification_window/types.hpp"

namespace ui::notification_window::render_context {

// 确保宿主窗口级 D2D 与 DComp 渲染资源已初始化
auto ensure_render_context(core::AppState& state) -> bool;

// 清理所有卡片的视觉表面并重置渲染上下文
auto cleanup_render_context(core::AppState& state) -> void;

// 确保指定卡片的 DComp 表面、Visual 与效果组已创建并挂载
auto ensure_card(core::AppState& state, Notification& notification) -> bool;

// 将卡片的位移与透明度运动状态应用到 DComp Visual 属性
auto apply_motion(core::AppState& state, Notification& notification) -> bool;

// 从 Visual 树中移除卡片并释放其合成资源
auto remove_card(core::AppState& state, Notification& notification) -> void;

// 提交当前 DComp 设备上的所有事务变更到系统合成器
auto commit(core::AppState& state) -> bool;

}  // namespace ui::notification_window::render_context
