#pragma once

#include <d2d1_3.h>
#include <windows.h>

#include "core/state/app_state.hpp"

namespace UI::SharedTheme {

struct FloatingWindowThemeColors {
  D2D1_COLOR_F background{};
  D2D1_COLOR_F separator{};
  D2D1_COLOR_F text{};
  D2D1_COLOR_F indicator{};
  D2D1_COLOR_F hover{};
  D2D1_COLOR_F title_bar{};
  D2D1_COLOR_F scroll_indicator{};
};

auto resolve_floating_window_theme_colors(const Core::State::AppState& state)
    -> FloatingWindowThemeColors;

}  // namespace UI::SharedTheme
