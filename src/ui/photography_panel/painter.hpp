#pragma once

#include <windows.h>

#include "core/state/app_state.hpp"
#include "ui/photography_panel/state.hpp"

namespace UI::PhotographyPanel::Painter {

auto compute_panel_layout(const Core::State::AppState& state)
    -> UI::PhotographyPanel::State::PanelLayoutMetrics;
auto shutter_to_x(const RECT& rect, int frames) -> float;
auto paint(Core::State::AppState& state, HWND hwnd) -> void;

}  // namespace UI::PhotographyPanel::Painter
