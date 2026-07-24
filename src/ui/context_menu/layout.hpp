#pragma once

#include "core/state/app_state.hpp"
#include "ui/context_menu/state.hpp"
#include "vendor/windows.hpp"

namespace UI::ContextMenu::Layout {

// 计算菜单总尺寸
auto calculate_menu_size(Core::State::AppState& app_state) -> void;

// 计算菜单显示位置（确保在屏幕内）
auto calculate_menu_position(const Core::State::AppState& app_state,
                             const Vendor::Windows::POINT& cursor_pos) -> Vendor::Windows::POINT;

// 计算文本宽度
auto calculate_text_width(const Core::State::AppState& app_state, const std::wstring& text) -> int;

// 根据坐标获取菜单项索引
auto get_menu_item_at_point(const Core::State::AppState& app_state,
                            const Vendor::Windows::POINT& pt) -> int;

// 子菜单计算函数
auto calculate_submenu_size(Core::State::AppState& app_state) -> void;
auto calculate_submenu_position(Core::State::AppState& app_state, int parent_index) -> void;

}  // namespace UI::ContextMenu::Layout