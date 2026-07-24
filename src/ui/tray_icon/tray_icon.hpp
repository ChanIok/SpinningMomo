#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace ui::tray_icon {

auto create(core::AppState& state) -> std::expected<void, std::string>;

auto destroy(core::AppState& state) -> void;

auto show_context_menu(core::AppState& state) -> void;

}  // namespace ui::tray_icon
