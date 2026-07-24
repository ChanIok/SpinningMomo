#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"

namespace UI::PhotographyPanel::RenderContext {

auto ensure_render_context(Core::State::AppState& state) -> bool;
auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool;
auto cleanup_render_context(Core::State::AppState& state) -> void;
auto update_theme_brushes(Core::State::AppState& state) -> void;
auto update_text_format(Core::State::AppState& state) -> bool;

}  // namespace UI::PhotographyPanel::RenderContext
