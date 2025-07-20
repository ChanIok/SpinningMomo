module;

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <iostream>

module Features.Notifications;

import std;
import Core.State;
import UI.AppWindow.State;
import Features.Notifications.State;
import Features.Notifications.Constants;
import Utils.Logger;

namespace Features::Notifications {

auto ease_out_cubic(float t) -> float {
  float ft = 1.0f - t;
  return 1.0f - ft * ft * ft;
}

auto calculate_window_height(const std::wstring& message, int dpi) -> int {
  HDC hdc = GetDC(NULL);
  if (!hdc) return Constants::BASE_MIN_HEIGHT;

  // 缩放字体大小
  int fontSize = MulDiv(Constants::BASE_FONT_SIZE, dpi, 96);
  int titleHeight = MulDiv(Constants::BASE_TITLE_HEIGHT, dpi, 96);
  int padding = MulDiv(Constants::BASE_PADDING, dpi, 96);
  int contentPadding = MulDiv(Constants::BASE_CONTENT_PADDING, dpi, 96);
  int windowWidth = MulDiv(Constants::BASE_WINDOW_WIDTH, dpi, 96);

  HFONT messageFont = CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

  HFONT oldFont = (HFONT)SelectObject(hdc, messageFont);

  int textWidth = windowWidth - (contentPadding * 2);

  RECT textRect = {0, 0, textWidth, 0};
  UINT format = DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS;
  DrawText(hdc, message.c_str(), -1, &textRect, format);
  int contentHeight = textRect.bottom - textRect.top;

  SelectObject(hdc, oldFont);
  DeleteObject(messageFont);
  ReleaseDC(NULL, hdc);

  int totalHeight = titleHeight + contentHeight + (padding * 2);

  int minHeight = MulDiv(Constants::BASE_MIN_HEIGHT, dpi, 96);
  int maxHeight = MulDiv(Constants::BASE_MAX_HEIGHT, dpi, 96);

  return std::clamp(totalHeight, minHeight, maxHeight);
}

void draw_close_button(HDC hdc, const RECT& rect, bool is_hovered, int dpi) {
  int close_size = MulDiv(Constants::BASE_CLOSE_SIZE, dpi, 96);
  int close_padding = MulDiv(Constants::BASE_CLOSE_PADDING, dpi, 96);
  int padding = MulDiv(Constants::BASE_PADDING, dpi, 96);

  int x = rect.right - close_size - close_padding;
  int y = padding;

  int penWidth = std::max(2, dpi / 48);

  COLORREF closeColor = is_hovered ? Constants::CLOSE_HOVER_COLOR : Constants::CLOSE_NORMAL_COLOR;
  LOGBRUSH lb = {BS_SOLID, closeColor, 0};
  HPEN hClosePen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, penWidth,
                                &lb, 0, nullptr);

  HGDIOBJ oldPen = SelectObject(hdc, hClosePen);

  int cross_padding = penWidth + 1;
  int effectiveSize = close_size - (cross_padding * 2);

  MoveToEx(hdc, x + cross_padding, y + cross_padding, NULL);
  LineTo(hdc, x + cross_padding + effectiveSize, y + cross_padding + effectiveSize);
  MoveToEx(hdc, x + cross_padding + effectiveSize, y + cross_padding, NULL);
  LineTo(hdc, x + cross_padding, y + cross_padding + effectiveSize);

  SelectObject(hdc, oldPen);
  DeleteObject(hClosePen);
}

void on_paint(HDC hdc, Features::Notifications::State::Notification& n, int dpi) {
  RECT rect;
  GetClientRect(n.hwnd, &rect);

  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
  HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

  SetBkMode(memDC, TRANSPARENT);

  int title_font_size = MulDiv(Constants::BASE_TITLE_FONT_SIZE, dpi, 96);
  int font_size = MulDiv(Constants::BASE_FONT_SIZE, dpi, 96);
  int padding = MulDiv(Constants::BASE_PADDING, dpi, 96);
  int title_height = MulDiv(Constants::BASE_TITLE_HEIGHT, dpi, 96);
  int content_padding = MulDiv(Constants::BASE_CONTENT_PADDING, dpi, 96);
  int close_padding_h = MulDiv(Constants::BASE_CLOSE_PADDING, dpi, 96);

  HFONT titleFont = CreateFont(-title_font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
  HFONT messageFont = CreateFont(-font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

  HBRUSH hBackBrush = CreateSolidBrush(Constants::BG_COLOR);
  FillRect(memDC, &rect, hBackBrush);
  DeleteObject(hBackBrush);

  int textLeft = content_padding;
  int textRight = rect.right - close_padding_h;

  SelectObject(memDC, titleFont);
  SetTextColor(memDC, Constants::TITLE_COLOR);
  RECT titleRect = {textLeft, padding, textRight, padding + title_height};
  DrawText(memDC, n.title.c_str(), -1, &titleRect,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

  SelectObject(memDC, messageFont);
  SetTextColor(memDC, Constants::TEXT_COLOR);
  RECT messageRect = {textLeft, padding + title_height, textRight, rect.bottom - padding};
  DrawText(memDC, n.message.c_str(), -1, &messageRect,
           DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS);

  draw_close_button(memDC, rect, n.is_close_hovered, dpi);

  BLENDFUNCTION blend = {AC_SRC_OVER, 0, static_cast<BYTE>(n.opacity * 255), 0};
  POINT ptSrc = {0, 0};
  SIZE sizeWnd = {rect.right - rect.left, rect.bottom - rect.top};
  UpdateLayeredWindow(n.hwnd, hdc, NULL, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

  SelectObject(memDC, oldBitmap);
  DeleteObject(titleFont);
  DeleteObject(messageFont);
  DeleteObject(memBitmap);
  DeleteDC(memDC);
}

// 窗口消息处理
LRESULT CALLBACK NotificationWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  Features::Notifications::State::Notification* notification = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    notification =
        reinterpret_cast<Features::Notifications::State::Notification*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(notification));
    notification->hwnd = hwnd;
  } else {
    notification = reinterpret_cast<Features::Notifications::State::Notification*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (!notification) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      on_paint(hdc, *notification, GetDpiForWindow(hwnd));
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_MOUSEMOVE: {
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);

      int dpi = GetDpiForWindow(hwnd);
      int close_size = MulDiv(Constants::BASE_CLOSE_SIZE, dpi, 96);
      int close_padding_h = MulDiv(Constants::BASE_CLOSE_PADDING, dpi, 96);
      int padding_v = MulDiv(Constants::BASE_PADDING, dpi, 96);

      RECT client_rect;
      GetClientRect(hwnd, &client_rect);
      RECT close_rect = {client_rect.right - close_size - close_padding_h, padding_v,
                         client_rect.right - close_padding_h, padding_v + close_size};

      bool current_close_hover = PtInRect(&close_rect, {x, y});
      if (notification->is_close_hovered != current_close_hover) {
        notification->is_close_hovered = current_close_hover;
        InvalidateRect(hwnd, NULL, TRUE);
      }

      if (!notification->is_hovered) {
        notification->is_hovered = true;
        if (notification->state ==
            Features::Notifications::State::NotificationAnimState::Displaying) {
          notification->pause_start_time = std::chrono::steady_clock::now();
        }

        TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
      }
      return 0;
    }

    case WM_MOUSELEAVE: {
      if (notification->is_close_hovered) {
        notification->is_close_hovered = false;
        InvalidateRect(hwnd, NULL, TRUE);
      }
      notification->is_hovered = false;
      if (notification->state ==
              Features::Notifications::State::NotificationAnimState::Displaying &&
          notification->pause_start_time.time_since_epoch().count() > 0) {
        auto paused_duration = std::chrono::steady_clock::now() - notification->pause_start_time;
        notification->total_paused_duration +=
            std::chrono::duration_cast<std::chrono::milliseconds>(paused_duration);
        notification->pause_start_time = {};  // Reset
      }
      InvalidateRect(hwnd, NULL, TRUE);
      return 0;
    }

    case WM_LBUTTONUP: {
      notification->state = Features::Notifications::State::NotificationAnimState::FadingOut;
      notification->last_state_change_time = std::chrono::steady_clock::now();
      return 0;
    }

    case WM_DESTROY: {
      return 0;
    }
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

auto register_window_class(HINSTANCE hInstance) -> void {
  static bool registered = false;
  if (registered) return;

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.lpfnWndProc = NotificationWindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = Constants::NOTIFICATION_WINDOW_CLASS.c_str();
  wc.hbrBackground = NULL;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClassEx(&wc);
  registered = true;
}

auto create_notification_window(Core::State::AppState& state,
                                Features::Notifications::State::Notification& notification)
    -> bool {
  register_window_class(state.app_window->window.instance);

  int dpi = state.app_window->window.dpi;
  int width = MulDiv(Constants::BASE_WINDOW_WIDTH, dpi, 96);

  HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              Constants::NOTIFICATION_WINDOW_CLASS.c_str(), L"Notification",
                              WS_POPUP | WS_CLIPCHILDREN, 0, 0, width, notification.height, NULL,
                              NULL, state.app_window->window.instance, &notification);

  if (!hwnd) return false;

  // 应用现代窗口样式
  MARGINS margins = {1, 1, 1, 1};
  DwmExtendFrameIntoClientArea(hwnd, &margins);
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  // HWND 存储在 notification 结构体中，通过 WM_NCCREATE 传递
  return true;
}

auto update_window_positions(Core::State::AppState& state) -> void {
  RECT workArea;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

  int dpi = state.app_window->window.dpi;
  int window_width = MulDiv(Constants::BASE_WINDOW_WIDTH, dpi, 96);
  int padding = MulDiv(Constants::BASE_PADDING, dpi, 96);
  int spacing = MulDiv(Constants::BASE_SPACING, dpi, 96);

  int current_y = workArea.bottom - padding;
  int base_x = workArea.right - window_width - padding;

  // 从下往上迭代，定位窗口
  for (auto it = state.notifications.active_notifications.rbegin();
       it != state.notifications.active_notifications.rend(); ++it) {
    auto& n = *it;
    if (n.state == Features::Notifications::State::NotificationAnimState::Done) continue;

    current_y -= n.height;

    POINT new_target_pos = {base_x, current_y};

    if (n.target_pos.y != new_target_pos.y &&
        n.state == Features::Notifications::State::NotificationAnimState::Displaying) {
      n.state = Features::Notifications::State::NotificationAnimState::MovingUp;
      n.last_state_change_time = std::chrono::steady_clock::now();
    }

    n.target_pos = new_target_pos;

    if (n.state == Features::Notifications::State::NotificationAnimState::Spawning) {
      // 新通知从底部滑入，起始位置不同
      n.current_pos = {base_x, workArea.bottom};
    } else if (n.state != Features::Notifications::State::NotificationAnimState::MovingUp) {
      // 确保 current_pos 在其他状态时更新，除非正在动画
      n.current_pos = n.target_pos;
    }

    current_y -= spacing;
  }
}

// 公共接口
auto show_notification(Core::State::AppState& state, const std::wstring& title,
                       const std::wstring& message) -> void {
  // 1. 如果活动通知数量达到上限，将最旧的标记为淡出
  if (state.notifications.active_notifications.size() >= Constants::MAX_VISIBLE_NOTIFICATIONS) {
    // 找到最旧的 "Displaying" 通知，并将其状态设置为 FadingOut
    for (auto& n : state.notifications.active_notifications) {
      if (n.state == Features::Notifications::State::NotificationAnimState::Displaying) {
        n.state = Features::Notifications::State::NotificationAnimState::FadingOut;
        n.last_state_change_time = std::chrono::steady_clock::now();
        break;  // 只淡出一个
      }
    }
  }

  // 2. 创建新的通知对象
  state.notifications.active_notifications.emplace_back(
      Features::Notifications::State::Notification{
          .id = state.notifications.next_id++,
          .title = title,
          .message = message,
          .state = Features::Notifications::State::NotificationAnimState::Spawning,
          .last_state_change_time = std::chrono::steady_clock::now()});

  // 在更新位置前计算高度
  auto& new_notification = state.notifications.active_notifications.back();
  new_notification.height =
      calculate_window_height(new_notification.message, state.app_window->window.dpi);

  // 3. 更新所有通知的位置
  update_window_positions(state);
}

// std::string 重载版本
auto show_notification(Core::State::AppState& state, const std::string& title,
                       const std::string& message) -> void {
  // 转换为 wstring 并调用主实现
  std::wstring wideTitle(title.begin(), title.end());
  std::wstring wideMessage(message.begin(), message.end());
  show_notification(state, wideTitle, wideMessage);
}

auto update_notifications(Core::State::AppState& state) -> void {
  auto now = std::chrono::steady_clock::now();

  // 遍历所有通知，根据其状态更新状态
  for (auto it = state.notifications.active_notifications.begin();
       it != state.notifications.active_notifications.end();
       /* no increment */) {
    auto& notification = *it;
    bool should_erase = false;

    auto elapsed_time = now - notification.last_state_change_time;
    if (notification.state == Features::Notifications::State::NotificationAnimState::Displaying &&
        notification.is_hovered) {
      // 悬停时不计时间
    } else if (notification.state ==
               Features::Notifications::State::NotificationAnimState::Displaying) {
      elapsed_time -= notification.total_paused_duration;
    }

    switch (notification.state) {
      case Features::Notifications::State::NotificationAnimState::Spawning:
        if (create_notification_window(state, notification)) {
          notification.state = Features::Notifications::State::NotificationAnimState::SlidingIn;
          notification.last_state_change_time = now;
          notification.total_paused_duration = std::chrono::milliseconds(0);
          ShowWindow(notification.hwnd, SW_SHOWNA);
          UpdateWindow(notification.hwnd);
        } else {
          // 创建失败，删除它
          should_erase = true;
        }
        break;
      case Features::Notifications::State::NotificationAnimState::SlidingIn: {
        float progress = std::min(
            1.0f, static_cast<float>(
                      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()) /
                      Constants::SLIDE_DURATION.count());
        float eased_progress = ease_out_cubic(progress);

        int new_y = notification.current_pos.y +
                    static_cast<int>((notification.target_pos.y - notification.current_pos.y) *
                                     eased_progress);

        SetWindowPos(notification.hwnd, HWND_TOPMOST, notification.target_pos.x, new_y, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
        notification.opacity = eased_progress;

        if (progress >= 1.0f) {
          notification.state = Features::Notifications::State::NotificationAnimState::Displaying;
          notification.last_state_change_time = now;
          notification.current_pos = notification.target_pos;
          notification.total_paused_duration = std::chrono::milliseconds(0);
        }
        InvalidateRect(notification.hwnd, NULL, TRUE);
        break;
      }
      case Features::Notifications::State::NotificationAnimState::Displaying:
        if (!notification.is_hovered && elapsed_time >= Constants::DISPLAY_DURATION) {
          notification.state = Features::Notifications::State::NotificationAnimState::FadingOut;
          notification.last_state_change_time = now;
        }
        break;
      case Features::Notifications::State::NotificationAnimState::MovingUp: {
        float progress = std::min(
            1.0f, static_cast<float>(
                      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()) /
                      Constants::SLIDE_DURATION.count());
        float eased_progress = ease_out_cubic(progress);

        int new_y = notification.current_pos.y +
                    static_cast<int>((notification.target_pos.y - notification.current_pos.y) *
                                     eased_progress);

        SetWindowPos(notification.hwnd, HWND_TOPMOST, notification.target_pos.x, new_y, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);

        if (progress >= 1.0f) {
          notification.state = Features::Notifications::State::NotificationAnimState::Displaying;
          notification.last_state_change_time = now;
          notification.current_pos = notification.target_pos;
        }
        break;
      }
      case Features::Notifications::State::NotificationAnimState::FadingOut: {
        float progress = std::min(
            1.0f, static_cast<float>(
                      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()) /
                      Constants::FADE_DURATION.count());
        notification.opacity = 1.0f - ease_out_cubic(progress);

        if (progress >= 1.0f) {
          notification.state = Features::Notifications::State::NotificationAnimState::Done;
        }
        InvalidateRect(notification.hwnd, NULL, TRUE);
        break;
      }
      case Features::Notifications::State::NotificationAnimState::Done:
        // 标记为待删除
        should_erase = true;
        break;
    }

    if (should_erase) {
      if (notification.hwnd) {
        DestroyWindow(notification.hwnd);
      }
      it = state.notifications.active_notifications.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace Features::Notifications