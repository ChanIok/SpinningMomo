#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "ui/photography_panel/state.hpp"

namespace ui::photography_panel::painter {

auto compute_panel_layout(const core::AppState& state) -> ui::photography_panel::PanelLayoutMetrics;
auto shutter_to_x(const RECT& rect, int frames) -> float;
auto paint(core::AppState& state, HWND hwnd) -> void;

}  // namespace ui::photography_panel::painter
