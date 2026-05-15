module;

export module Utils.DisplayGeometry;

import <windows.h>;

namespace Utils::DisplayGeometry {

// 根据目标可见视口与相对位置，计算窗口左上角。
// 当窗口大于视口时，优先让该相对位置对应的窗口点落在视口中心，
// 然后再限制到“视口始终落在窗口内部”的范围内；
// 当窗口小于等于视口时，该轴自动居中。
export auto calculate_window_position_for_viewport(const RECT& viewport_rect, int window_width,
                                                   int window_height, double relative_x,
                                                   double relative_y) -> POINT;

}  // namespace Utils::DisplayGeometry
