module;

export module UI.ContextMenu;

import std;
import Core.State;
import UI.ContextMenu.Types;
import Vendor.Windows;

namespace UI::ContextMenu {

export auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;

export auto cleanup(Core::State::AppState& state) -> void;

export auto Show(Core::State::AppState& state, std::vector<Types::MenuItem> items,
                 const Vendor::Windows::POINT& position) -> void;

export auto hide_and_destroy_menu(Core::State::AppState& state) -> void;

export auto hide_submenu(Core::State::AppState& state) -> void;

export auto show_submenu(Core::State::AppState& state, int index) -> void;

export auto handle_menu_action(Core::State::AppState& state,
                               const UI::ContextMenu::Types::MenuItem& item) -> void;

}  // namespace UI::ContextMenu
