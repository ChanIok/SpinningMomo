module;

module UI.SharedTheme;

import std;
import Core.State;
import Features.Settings.State;
import <d2d1_3.h>;

namespace UI::SharedTheme {

auto hex_char_to_int(char c) -> int {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

auto parse_hex_color(std::string_view hex_color, D2D1_COLOR_F fallback) -> D2D1_COLOR_F {
  if (hex_color.empty()) {
    return fallback;
  }
  if (hex_color.starts_with('#')) {
    hex_color.remove_prefix(1);
  }
  if (hex_color.size() < 6) {
    return fallback;
  }

  const int r_hi = hex_char_to_int(hex_color[0]);
  const int r_lo = hex_char_to_int(hex_color[1]);
  const int g_hi = hex_char_to_int(hex_color[2]);
  const int g_lo = hex_char_to_int(hex_color[3]);
  const int b_hi = hex_char_to_int(hex_color[4]);
  const int b_lo = hex_char_to_int(hex_color[5]);
  if (r_hi < 0 || r_lo < 0 || g_hi < 0 || g_lo < 0 || b_hi < 0 || b_lo < 0) {
    return fallback;
  }

  float alpha = fallback.a;
  if (hex_color.size() >= 8) {
    const int a_hi = hex_char_to_int(hex_color[6]);
    const int a_lo = hex_char_to_int(hex_color[7]);
    if (a_hi >= 0 && a_lo >= 0) {
      alpha = static_cast<float>((a_hi << 4) | a_lo) / 255.0f;
    }
  } else {
    alpha = 1.0f;
  }

  return D2D1::ColorF(static_cast<float>((r_hi << 4) | r_lo) / 255.0f,
                      static_cast<float>((g_hi << 4) | g_lo) / 255.0f,
                      static_cast<float>((b_hi << 4) | b_lo) / 255.0f, alpha);
}

// 把浮窗设置里的颜色统一解析成 D2D token，避免各个窗口各自解释同名配置
auto resolve_floating_window_theme_colors(const Core::State::AppState& state)
    -> FloatingWindowThemeColors {
  FloatingWindowThemeColors colors{
      .background = D2D1::ColorF(0.12f, 0.12f, 0.12f, 0.70f),
      .separator = D2D1::ColorF(0.20f, 0.20f, 0.20f, 0.70f),
      .text = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f),
      .indicator = D2D1::ColorF(0.98f, 0.75f, 0.14f, 1.0f),
      .hover = D2D1::ColorF(0.31f, 0.31f, 0.31f, 0.80f),
      .title_bar = D2D1::ColorF(0.12f, 0.12f, 0.12f, 0.70f),
      .scroll_indicator = D2D1::ColorF(0.50f, 0.50f, 0.50f, 0.80f),
  };

  // 先给一套稳定默认值，避免设置缺项时把整个 UI 颜色打成透明或黑块
  const auto& settings_colors = state.settings->raw.ui.floating_window_colors;
  colors.background = parse_hex_color(settings_colors.background, colors.background);
  colors.separator = parse_hex_color(settings_colors.separator, colors.separator);
  colors.text = parse_hex_color(settings_colors.text, colors.text);
  colors.indicator = parse_hex_color(settings_colors.indicator, colors.indicator);
  colors.hover = parse_hex_color(settings_colors.hover, colors.hover);
  colors.title_bar = parse_hex_color(settings_colors.title_bar, colors.title_bar);
  colors.scroll_indicator =
      parse_hex_color(settings_colors.scroll_indicator, colors.scroll_indicator);
  return colors;
}

}  // namespace UI::SharedTheme
