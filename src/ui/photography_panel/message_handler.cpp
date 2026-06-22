module;

module UI.PhotographyPanel.MessageHandler;

import std;
import Core.State;
import Features.Photography.LongExposure;
import Features.Photography.State;
import Features.Photography.UseCase;
import UI.PhotographyPanel.Painter;
import UI.PhotographyPanel.RenderContext;
import UI.PhotographyPanel.State;
import <dwmapi.h>;
import <windows.h>;
import <windowsx.h>;

namespace UI::PhotographyPanel::MessageHandler {

enum class SliderHitPart {
  None,
  Track,
  Knob,
};

auto ratio_from_x(const RECT& rect, int x) -> float {
  const int width = std::max<LONG>(1, rect.right - rect.left);
  return std::clamp(static_cast<float>(x - rect.left) / static_cast<float>(width), 0.0f, 1.0f);
}

// 将鼠标 X 坐标映射到最近的帧数档位（0/30/60/120/300/1000）
auto shutter_from_x(const RECT& rect, int x) -> int {
  const float ratio = ratio_from_x(rect, x);
  const auto stops = Features::Photography::LongExposure::frame_stops();
  const int max_index = static_cast<int>(stops.size()) - 1;
  const int index = std::clamp(static_cast<int>(std::round(ratio * max_index)), 0, max_index);
  return stops[static_cast<std::size_t>(index)];
}

auto point_hits_slider_knob(const RECT& rect, int frames, POINT point) -> bool {
  const float knob_x = UI::PhotographyPanel::Painter::shutter_to_x(rect, frames);
  const float knob_y = static_cast<float>(rect.top + rect.bottom) * 0.5f;
  const float dx = static_cast<float>(point.x) - knob_x;
  const float dy = static_cast<float>(point.y) - knob_y;
  return dx * dx + dy * dy <= State::kSliderKnobRadius * State::kSliderKnobRadius;
}

auto point_hits_slider_track(const RECT& rect, POINT point) -> bool {
  const int center_y = (rect.top + rect.bottom) / 2;
  return point.x >= rect.left && point.x <= rect.right &&
         std::abs(point.y - center_y) <= State::kSliderTrackHitHalfHeight;
}

// 按实际绘制几何做命中测试，避免两端露出轨道外的旋钮看起来能点却抓不住
auto hit_test_slider(const Core::State::AppState& state, POINT point) -> SliderHitPart {
  const RECT rect = UI::PhotographyPanel::Painter::compute_panel_layout(state).slider_rect;
  const int frames = state.photography->shutter_frames.load(std::memory_order_acquire);
  if (point_hits_slider_knob(rect, frames, point)) {
    return SliderHitPart::Knob;
  }
  if (point_hits_slider_track(rect, point)) {
    return SliderHitPart::Track;
  }
  return SliderHitPart::None;
}

// 只给旋钮保留 hover 视觉状态，避免轨道区域也抢走交互重点
auto update_knob_hover_state(Core::State::AppState& state, SliderHitPart hit_part) -> void {
  const bool hovered = hit_part == SliderHitPart::Knob;
  if (state.photography_panel->knob_hovered == hovered) {
    return;
  }

  state.photography_panel->knob_hovered = hovered;
  if (state.photography_panel->hwnd && state.photography_panel->is_visible) {
    InvalidateRect(state.photography_panel->hwnd, nullptr, FALSE);
  }
}

// 根据鼠标位置更新帧数，值变化时刷新面板
auto update_long_exposure_from_x(Core::State::AppState& state, int x) -> void {
  const RECT rect = UI::PhotographyPanel::Painter::compute_panel_layout(state).slider_rect;
  const int next_frames =
      Features::Photography::LongExposure::nearest_frame_stop(shutter_from_x(rect, x));
  const int current = state.photography->shutter_frames.load(std::memory_order_acquire);
  if (next_frames == current) {
    return;
  }
  state.photography->shutter_frames.store(next_frames, std::memory_order_release);

  if (state.photography_panel->hwnd && state.photography_panel->is_visible) {
    InvalidateRect(state.photography_panel->hwnd, nullptr, FALSE);
  }
}

// 关闭面板：禁用摄影模式 → 隐藏窗口 → 刷新悬浮窗状态
auto close_panel(Core::State::AppState& state) -> void {
  Features::Photography::UseCase::handle_panel_close(state);
  if (state.photography_panel->hwnd) {
    ShowWindow(state.photography_panel->hwnd, SW_HIDE);
  }
  state.photography_panel->is_visible = false;
  state.photography_panel->dragging_long_exposure = false;
  state.photography_panel->knob_hovered = false;
}

// 面板窗口过程：标题栏拖拽、长曝光滑块拖动、D3D resize
auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM w_param,
                      LPARAM l_param) -> LRESULT {
  auto& panel = *state.photography_panel;
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps{};
      BeginPaint(hwnd, &ps);
      UI::PhotographyPanel::Painter::paint(state, hwnd);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      ScreenToClient(hwnd, &point);
      return point.y < UI::PhotographyPanel::Painter::compute_panel_layout(state).title_rect.bottom
                 ? HTCAPTION
                 : HTCLIENT;
    }

    case WM_LBUTTONDOWN: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};

      const auto hit_part = hit_test_slider(state, point);
      if (hit_part == SliderHitPart::None) {
        return 0;
      }

      // 点轨道和点旋钮都允许直接起拖，保证这个控件始终按可见几何响应输入
      panel.dragging_long_exposure = true;
      SetCapture(hwnd);
      update_long_exposure_from_x(state, point.x);
      return 0;
    }

    case WM_MOUSEMOVE: {
      const POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      update_knob_hover_state(state, hit_test_slider(state, point));
      // 只要鼠标还在窗口里，就持续追踪离开事件，保证 hover 不会卡住
      TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
      TrackMouseEvent(&tme);
      if (GetCapture() == hwnd && panel.dragging_long_exposure) {
        update_long_exposure_from_x(state, GET_X_LPARAM(l_param));
      }
      return 0;
    }

    case WM_MOUSELEAVE:
      update_knob_hover_state(state, SliderHitPart::None);
      return 0;

    case WM_LBUTTONUP:
      if (GetCapture() == hwnd) {
        ReleaseCapture();
      }
      panel.dragging_long_exposure = false;
      return 0;

    case WM_SIZE: {
      SIZE new_size{LOWORD(l_param), HIWORD(l_param)};
      if (UI::PhotographyPanel::RenderContext::resize_render_context(state, new_size)) {
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }

    case WM_CLOSE:
      close_panel(state);
      return 0;

    case WM_NCDESTROY:
      UI::PhotographyPanel::RenderContext::cleanup_render_context(state);
      panel.hwnd = nullptr;
      panel.is_visible = false;
      panel.dragging_long_exposure = false;
      panel.knob_hovered = false;
      return 0;
  }

  return DefWindowProcW(hwnd, msg, w_param, l_param);
}

// Win32 静态回调：通过 GWLP_USERDATA 把 HWND 绑定到 AppState，之后委托给 window_procedure
LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  Core::State::AppState* app_state = nullptr;

  if (msg == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
    app_state = static_cast<Core::State::AppState*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app_state));
    if (app_state && app_state->photography_panel) {
      app_state->photography_panel->hwnd = hwnd;
    }
  } else {
    app_state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (app_state && app_state->photography_panel) {
    return window_procedure(*app_state, hwnd, msg, w_param, l_param);
  }

  return DefWindowProcW(hwnd, msg, w_param, l_param);
}

}  // namespace UI::PhotographyPanel::MessageHandler
