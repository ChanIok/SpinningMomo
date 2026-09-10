#include "ui/notification_window/message_handler.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/windowsx.hpp"

#include "core/state/app_state.hpp"
#include "ui/notification_window/notification_window.hpp"
#include "ui/notification_window/render_context.hpp"
#include "ui/notification_window/state.hpp"
#include "ui/notification_window/types.hpp"

// 通知宿主窗口消息处理循环：命中穿透测试 → 鼠标输入分发 → 生命周期/悬停定时器处理 →
// DPI/显示变动适配
auto window_procedure(core::AppState& state, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
    -> LRESULT {
  switch (msg) {
    // 重绘消息：向合成器发布未提交的脏卡片内容与运动属性
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      BeginPaint(hwnd, &ps);
      ui::notification_window::publish_notifications(state);
      EndPaint(hwnd, &ps);
      return 0;
    }

    // 非卡片透明区域返回 HTTRANSPARENT 穿透给下层桌面，卡片区域返回 HTCLIENT 接收鼠标消息
    case WM_NCHITTEST: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      ScreenToClient(hwnd, &point);
      const auto target = ui::notification_window::hit_test_notifications(state, point);
      return target.kind == ui::notification_window::NotificationHitKind::None ? HTTRANSPARENT
                                                                               : HTCLIENT;
    }

    // 鼠标移动：通过瞬时物理坐标命中测试更新卡片与按钮悬停高亮
    case WM_MOUSEMOVE: {
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      const auto target = ui::notification_window::hit_test_notifications(state, point);
      ui::notification_window::update_hover_state(state, target);

      return 0;
    }

    // 鼠标离开宿主窗口：清空所有悬停状态
    case WM_MOUSELEAVE:
      ui::notification_window::update_hover_state(state, {});
      return 0;

    // 鼠标按下：记录按下目标并捕获鼠标输入
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

    // 鼠标抬起：释放输入捕获并分发点击事件
    case WM_LBUTTONUP: {
      if (GetCapture() == hwnd) {
        ReleaseCapture();
      }
      POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
      ui::notification_window::handle_click_release(
          state, ui::notification_window::hit_test_notifications(state, point));
      return 0;
    }

    // 定时器事件分发
    case WM_TIMER:
      // 处理卡片生命周期到期（展示倒计时结束或淡出完成）
      if (w_param == ui::notification_window::LIFECYCLE_TIMER_ID) {
        ui::notification_window::handle_lifecycle_timer(state);
        return 0;
      }
      // 卡片运动期间采样静止鼠标的悬停命中
      if (w_param == ui::notification_window::HOVER_TIMER_ID) {
        ui::notification_window::check_hover(state);
        return 0;
      }
      break;

    // 宿主窗口尺寸变化：标记排版脏
    case WM_SIZE:
      state.notification_window->layout_dirty = true;
      state.notification_window->host_size = {LOWORD(l_param), HIWORD(l_param)};
      return 0;

    // DPI 或多显示器工作区改变：重新计算宿主边界并推进排版
    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
      ui::notification_window::update_host_bounds(state);
      ui::notification_window::update_notifications(state);
      return 0;

    // 窗口非客户区销毁：清理 DComp 与 D2D 渲染上下文并重置定时器
    case WM_NCDESTROY:
      ui::notification_window::render_context::cleanup_render_context(state);
      state.notification_window->host_hwnd = nullptr;
      state.notification_window->scheduled_deadline.reset();
      state.notification_window->hover_timer_active = false;
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
