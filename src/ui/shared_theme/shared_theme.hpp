#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"

#include "core/state/app_state.hpp"

namespace ui::shared_theme {

struct FloatingWindowThemeColors {
  D2D1_COLOR_F background{};
  D2D1_COLOR_F separator{};
  D2D1_COLOR_F text{};
  D2D1_COLOR_F indicator{};
  D2D1_COLOR_F hover{};
  D2D1_COLOR_F title_bar{};
  D2D1_COLOR_F scroll_indicator{};
};

auto resolve_floating_window_theme_colors(const core::AppState& state) -> FloatingWindowThemeColors;

}  // namespace ui::shared_theme
