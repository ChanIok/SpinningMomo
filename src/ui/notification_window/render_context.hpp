#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace ui::notification_window::render_context {

auto ensure_render_context(core::AppState& state) -> bool;
auto cleanup_render_context(core::AppState& state) -> void;
auto resize_render_context(core::AppState& state, const SIZE& new_size) -> bool;

}  // namespace ui::notification_window::render_context
