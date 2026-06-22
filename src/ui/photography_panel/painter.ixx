module;

export module UI.PhotographyPanel.Painter;

import Core.State;
import UI.PhotographyPanel.State;
import <windows.h>;

namespace UI::PhotographyPanel::Painter {

export auto compute_panel_layout(const Core::State::AppState& state)
    -> UI::PhotographyPanel::State::PanelLayoutMetrics;
export auto shutter_to_x(const RECT& rect, int frames) -> float;
export auto paint(Core::State::AppState& state, HWND hwnd) -> void;

}  // namespace UI::PhotographyPanel::Painter
