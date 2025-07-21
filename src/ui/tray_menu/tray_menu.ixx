module;

export module UI.TrayMenu;

import std;
import Core.State;
import UI.TrayMenu.State;
import UI.TrayMenu.Types;
import Vendor.Windows;

namespace UI::TrayMenu {

// 初始化托盘菜单系统
export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;

// 清理托盘菜单系统
export auto cleanup(Core::State::AppState& state) -> void;

// 显示托盘菜单
export auto show_menu(Core::State::AppState& state, const Vendor::Windows::POINT& position) -> void;

// 隐藏托盘菜单
export auto hide_menu(Core::State::AppState& state) -> void;

// 检查菜单是否可见
export auto is_menu_visible(const Core::State::AppState& state) -> bool;

// 处理菜单命令 - 更新为接受MenuItem
export auto handle_menu_command(Core::State::AppState& state,
                                const UI::TrayMenu::Types::MenuItem& item) -> void;

// 子菜单管理
export auto show_submenu(Core::State::AppState& state, int parent_index) -> void;
export auto hide_submenu(Core::State::AppState& state) -> void;

}  // namespace UI::TrayMenu
