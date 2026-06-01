module;

#include <windows.h>
#include <windowsx.h>

module UI.NotificationWindow.MessageHandler;

import std;
import Core.State;
import UI.NotificationWindow;
import UI.NotificationWindow.Painter;
import UI.NotificationWindow.RenderContext;
import UI.NotificationWindow.State;
import UI.NotificationWindow.Types;

auto window_procedure(Core::State::AppState& state, HWND hwnd, UINT msg, WPARAM w_param,
                      LPARAM l_param) -> LRESULT {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      BeginPaint(hwnd, &ps);
      UI::NotificationWindow::Painter::paint_notifications(state);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      ScreenToClient(hwnd, &point);
      const auto target = UI::NotificationWindow::hit_test_notifications(state, point);
      return target.kind == UI::NotificationWindow::NotificationHitKind::None ? HTTRANSPARENT
                                                                              : HTCLIENT;
    }

    case WM_MOUSEMOVE: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      const auto target = UI::NotificationWindow::hit_test_notifications(state, point);
      if (UI::NotificationWindow::update_hover_state(state, target)) {
        UI::NotificationWindow::request_repaint(state);
      }

      TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
      TrackMouseEvent(&tme);
      return 0;
    }

    case WM_MOUSELEAVE:
      if (UI::NotificationWindow::update_hover_state(state, {})) {
        UI::NotificationWindow::request_repaint(state);
      }
      return 0;

    case WM_LBUTTONDOWN: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      const auto target = UI::NotificationWindow::hit_test_notifications(state, point);
      if (target.kind == UI::NotificationWindow::NotificationHitKind::Content ||
          target.kind == UI::NotificationWindow::NotificationHitKind::Action) {
        state.notification_window->pressed_target = target;
        SetCapture(hwnd);
        UI::NotificationWindow::request_repaint(state);
      }
      return 0;
    }

    case WM_LBUTTONUP: {
      if (GetCapture() == hwnd) {
        ReleaseCapture();
      }
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      UI::NotificationWindow::handle_click_release(
          state, UI::NotificationWindow::hit_test_notifications(state, point));
      return 0;
    }

    case WM_TIMER:
      if (w_param == UI::NotificationWindow::ANIMATION_TIMER_ID) {
        UI::NotificationWindow::update_notifications(state);
        return 0;
      }
      break;

    case WM_SIZE: {
      SIZE new_size{LOWORD(l_param), HIWORD(l_param)};
      if (UI::NotificationWindow::RenderContext::resize_render_context(state, new_size)) {
        UI::NotificationWindow::request_repaint(state);
      }
      return 0;
    }

    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
      UI::NotificationWindow::update_host_bounds(state);
      UI::NotificationWindow::relayout_notifications(state, std::chrono::steady_clock::now());
      UI::NotificationWindow::request_repaint(state);
      return 0;

    case WM_NCDESTROY:
      UI::NotificationWindow::RenderContext::cleanup_render_context(state);
      state.notification_window->host_hwnd = nullptr;
      state.notification_window->animation_timer_active = false;
      return 0;
  }

  return DefWindowProcW(hwnd, msg, w_param, l_param);
}

namespace UI::NotificationWindow::MessageHandler {

LRESULT CALLBACK static_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  Core::State::AppState* app_state = nullptr;

  if (msg == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
    app_state = static_cast<Core::State::AppState*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app_state));
    if (app_state) {
      app_state->notification_window->host_hwnd = hwnd;
    }
  } else {
    app_state = reinterpret_cast<Core::State::AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (app_state) {
    return window_procedure(*app_state, hwnd, msg, w_param, l_param);
  }

  return DefWindowProcW(hwnd, msg, w_param, l_param);
}

}  // namespace UI::NotificationWindow::MessageHandler
