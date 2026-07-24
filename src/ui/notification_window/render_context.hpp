#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"

namespace UI::NotificationWindow::RenderContext {

auto ensure_render_context(Core::State::AppState& state) -> bool;
auto cleanup_render_context(Core::State::AppState& state) -> void;
auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool;

}  // namespace UI::NotificationWindow::RenderContext
