#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace ui::shared_render_resources {

auto ensure_initialized(core::AppState& state) -> bool;
auto cleanup(core::AppState& state) -> void;

}  // namespace ui::shared_render_resources
