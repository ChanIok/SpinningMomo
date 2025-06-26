module;

#include <windows.h>

export module UI.TrayMenu.Layout;

import std;
import Core.State;
import Vendor.Windows;

namespace UI::TrayMenu::Layout {

// 计算菜单总尺寸
export auto calculate_menu_size(Core::State::AppState& state) -> void;

// 计算菜单显示位置（确保在屏幕内）
export auto calculate_menu_position(Core::State::AppState& state,
                                    const Vendor::Windows::POINT& cursor_pos)
    -> Vendor::Windows::POINT;

// 计算文本宽度
export auto calculate_text_width(const Core::State::AppState& state, const std::wstring& text)
    -> int;

// 根据坐标获取菜单项索引
export auto get_menu_item_at_point(const Core::State::AppState& state, const POINT& pt) -> int;

}  // namespace UI::TrayMenu::Layout