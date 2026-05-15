module;

module Utils.DisplayGeometry;

import std;
import <windows.h>;

namespace Utils::DisplayGeometry {

auto clamp_relative_position(double value) -> double {
  if (!std::isfinite(value)) {
    return 0.5;
  }
  return std::clamp(value, 0.0, 1.0);
}

auto calculate_window_position_for_viewport(const RECT& viewport_rect, int window_width,
                                            int window_height, double relative_x, double relative_y)
    -> POINT {
  relative_x = clamp_relative_position(relative_x);
  relative_y = clamp_relative_position(relative_y);

  const int viewport_width = viewport_rect.right - viewport_rect.left;
  const int viewport_height = viewport_rect.bottom - viewport_rect.top;
  const double viewport_center_x = viewport_rect.left + viewport_width / 2.0;
  const double viewport_center_y = viewport_rect.top + viewport_height / 2.0;

  double target_x = viewport_rect.left;
  if (window_width <= viewport_width) {
    target_x += (viewport_width - window_width) / 2.0;
  } else {
    const double desired_x = viewport_center_x - relative_x * window_width;
    const double min_x = static_cast<double>(viewport_rect.right - window_width);
    const double max_x = static_cast<double>(viewport_rect.left);
    target_x = std::clamp(desired_x, min_x, max_x);
  }

  double target_y = viewport_rect.top;
  if (window_height <= viewport_height) {
    target_y += (viewport_height - window_height) / 2.0;
  } else {
    const double desired_y = viewport_center_y - relative_y * window_height;
    const double min_y = static_cast<double>(viewport_rect.bottom - window_height);
    const double max_y = static_cast<double>(viewport_rect.top);
    target_y = std::clamp(desired_y, min_y, max_y);
  }

  return POINT{
      .x = static_cast<LONG>(std::lround(target_x)),
      .y = static_cast<LONG>(std::lround(target_y)),
  };
}

}  // namespace Utils::DisplayGeometry
