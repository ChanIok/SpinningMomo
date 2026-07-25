#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace features::photography {

auto start(core::AppState& state) -> std::expected<void, std::string>;
auto stop(core::AppState& state) -> void;
auto toggle(core::AppState& state) -> void;
auto cleanup(core::AppState& state) -> void;
auto handle_panel_close(core::AppState& state) -> void;

}  // namespace features::photography
