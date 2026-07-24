#pragma once

#include "core/state/app_state.hpp"

namespace UI::TrayIcon {

auto create(Core::State::AppState& state) -> std::expected<void, std::string>;

auto destroy(Core::State::AppState& state) -> void;

auto show_context_menu(Core::State::AppState& state) -> void;

}  // namespace UI::TrayIcon