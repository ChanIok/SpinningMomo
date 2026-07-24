#pragma once

#include "core/state/app_state.hpp"
#include "ui/context_menu/types.hpp"
#include "vendor/windows.hpp"

namespace UI::ContextMenu {

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string>;

auto cleanup(Core::State::AppState& state) -> void;

auto Show(Core::State::AppState& state, std::vector<Types::MenuItem> items,
          const Vendor::Windows::POINT& position) -> void;

auto hide_and_destroy_menu(Core::State::AppState& state) -> void;

auto hide_submenu(Core::State::AppState& state) -> void;

auto show_submenu(Core::State::AppState& state, int index) -> void;

auto handle_menu_action(Core::State::AppState& state, const UI::ContextMenu::Types::MenuItem& item)
    -> void;

}  // namespace UI::ContextMenu
