#include "ui/notification_window/message_handler.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/windowsx.hpp"

#include "core/state/app_state.hpp"
#include "ui/notification_window/notification_window.hpp"
#include "ui/notification_window/painter.hpp"
#include "ui/notification_window/render_context.hpp"
#include "ui/notification_window/state.hpp"
#include "ui/notification_window/types.hpp"

auto window_procedure(core::AppState& state, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
    -> LRESULT {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      BeginPaint(hwnd, &ps);
      ui::notification_window::painter::paint_notifications(state);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      ScreenToClient(hwnd, &point);
      const auto target = ui::notification_window::hit_test_notifications(state, point);
      return target.kind == ui::notification_window::NotificationHitKind::None ? HTTRANSPARENT
                                                                               : HTCLIENT;
    }

    case WM_MOUSEMOVE: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      const auto target = ui::notification_window::hit_test_notifications(state, point);
      if (ui::notification_window::update_hover_state(state, target)) {
        ui::notification_window::request_repaint(state);
      }

      TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
      TrackMouseEvent(&tme);
      return 0;
    }

    case WM_MOUSELEAVE:
      if (ui::notification_window::update_hover_state(state, {})) {
        ui::notification_window::request_repaint(state);
      }
      return 0;

    case WM_LBUTTONDOWN: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      const auto target = ui::notification_window::hit_test_notifications(state, point);
      if (target.kind == ui::notification_window::NotificationHitKind::Content ||
          target.kind == ui::notification_window::NotificationHitKind::Action) {
        state.notification_window->pressed_target = target;
        SetCapture(hwnd);
        ui::notification_window::request_repaint(state);
      }
      return 0;
    }

    case WM_LBUTTONUP: {
      if (GetCapture() == hwnd) {
        ReleaseCapture();
      }
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      ui::notification_window::handle_click_release(
          state, ui::notification_window::hit_test_notifications(state, point));
      return 0;
    }

    case WM_TIMER:
      if (w_param == ui::notification_window::ANIMATION_TIMER_ID) {
        ui::notification_window::update_notifications(state);
        return 0;
      }
      break;

    case WM_SIZE: {
      SIZE new_size{LOWORD(l_param), HIWORD(l_param)};
      if (ui::notification_window::render_context::resize_render_context(state, new_size)) {
        ui::notification_window::request_repaint(state);
      }
      return 0;
    }

    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
      ui::notification_window::update_host_bounds(state);
      ui::notification_window::relayout_notifications(state, std::chrono::steady_clock::now());
      ui::notification_window::request_repaint(state);
      return 0;

    case WM_NCDESTROY:
      ui::notification_window::render_context::cleanup_render_context(state);
      state.notification_window->host_hwnd = nullptr;
      state.notification_window->animation_timer_active = false;
      return 0;
  }

  return DefWindowProcW(hwnd, msg, w_param, l_param);
}

namespace ui::notification_window::message_handler {

LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  core::AppState* app_state = nullptr;

  if (msg == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
    app_state = static_cast<core::AppState*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app_state));
    if (app_state) {
      app_state->notification_window->host_hwnd = hwnd;
    }
  } else {
    app_state = reinterpret_cast<core::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (app_state) {
    return window_procedure(*app_state, hwnd, msg, w_param, l_param);
  }

  return DefWindowProcW(hwnd, msg, w_param, l_param);
}

}  // namespace ui::notification_window::message_handler
