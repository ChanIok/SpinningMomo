#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace ui::photography_panel::render_context {

auto ensure_render_context(core::AppState& state) -> bool;
auto resize_render_context(core::AppState& state, const SIZE& new_size) -> bool;
auto cleanup_render_context(core::AppState& state) -> void;
auto update_theme_brushes(core::AppState& state) -> void;
auto update_text_format(core::AppState& state) -> bool;

}  // namespace ui::photography_panel::render_context
