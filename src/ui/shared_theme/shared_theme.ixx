module;

export module UI.SharedTheme;

import Core.State;
import <d2d1_3.h>;

namespace UI::SharedTheme {

export struct FloatingWindowThemeColors {
  D2D1_COLOR_F background{};
  D2D1_COLOR_F separator{};
  D2D1_COLOR_F text{};
  D2D1_COLOR_F indicator{};
  D2D1_COLOR_F hover{};
  D2D1_COLOR_F title_bar{};
  D2D1_COLOR_F scroll_indicator{};
};

export auto resolve_floating_window_theme_colors(const Core::State::AppState& state)
    -> FloatingWindowThemeColors;

}  // namespace UI::SharedTheme
