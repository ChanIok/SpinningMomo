module;

#include <windows.h>
#include <windowsx.h>

#include <iostream>

module Features.Preview.Interaction;

import std;
import Features.Preview.State;
import Core.State;
import Core.Constants;
import Utils.Logger;
import Features.Preview.Rendering;

namespace Features::Preview::Interaction {

// 辅助函数实现
auto is_point_in_title_bar(const Core::State::AppState& state, POINT pt) -> bool {
  return pt.y < state.preview->dpi_sizes.title_height;
}

auto is_point_in_viewport(const Core::State::AppState& state, POINT pt) -> bool {
  return (pt.x >= state.preview->viewport.viewport_rect.left &&
          pt.x <= state.preview->viewport.viewport_rect.right &&
          pt.y >= state.preview->viewport.viewport_rect.top &&
          pt.y <= state.preview->viewport.viewport_rect.bottom);
}

auto get_border_hit_test(const Core::State::AppState& state, HWND hwnd, POINT pt) -> LRESULT {
  RECT rc;
  GetClientRect(hwnd, &rc);

  int borderWidth = state.preview->dpi_sizes.border_width;

  if (pt.x <= borderWidth) {
    if (pt.y <= borderWidth) return HTTOPLEFT;
    if (pt.y >= rc.bottom - borderWidth) return HTBOTTOMLEFT;
    return HTLEFT;
  }
  if (pt.x >= rc.right - borderWidth) {
    if (pt.y <= borderWidth) return HTTOPRIGHT;
    if (pt.y >= rc.bottom - borderWidth) return HTBOTTOMRIGHT;
    return HTRIGHT;
  }
  if (pt.y <= borderWidth) return HTTOP;
  if (pt.y >= rc.bottom - borderWidth) return HTBOTTOM;

  return HTCLIENT;
}

auto move_game_window_to_position(Core::State::AppState& state, float relative_x, float relative_y)
    -> void {
  if (!state.preview->target_window) return;

  Logger().debug("move_game_window_to_position: relative_x: {}, relative_y: {}", relative_x,
                relative_y);

  // 获取屏幕尺寸
  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  // 获取游戏窗口尺寸
  float gameWidth = static_cast<float>(state.preview->game_window_rect.right -
                                       state.preview->game_window_rect.left);
  float gameHeight = static_cast<float>(state.preview->game_window_rect.bottom -
                                        state.preview->game_window_rect.top);

  // 计算新位置
  float targetX, targetY;

  // 水平方向
  if (gameWidth <= screenWidth) {
    targetX = (screenWidth - gameWidth) / 2;  // 居中
  } else {
    targetX = -relative_x * gameWidth + screenWidth / 2;
    targetX = std::max(targetX, -gameWidth + screenWidth);
    targetX = std::min(targetX, 0.0f);
  }

  // 垂直方向
  if (gameHeight <= screenHeight) {
    targetY = (screenHeight - gameHeight) / 2;  // 居中
  } else {
    targetY = -relative_y * gameHeight + screenHeight / 2;
    targetY = std::max(targetY, -gameHeight + screenHeight);
    targetY = std::min(targetY, 0.0f);
  }

  // 移动游戏窗口
  SetWindowPos(state.preview->target_window, nullptr, static_cast<int>(targetX),
               static_cast<int>(targetY), 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
}

// 窗口拖拽实现
auto start_window_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void {
  state.preview->interaction.is_dragging = true;
  state.preview->interaction.drag_start = pt;
  SetCapture(hwnd);
}

auto update_window_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void {
  if (!state.preview->interaction.is_dragging) return;

  RECT rect;
  GetWindowRect(hwnd, &rect);
  int deltaX = pt.x - state.preview->interaction.drag_start.x;
  int deltaY = pt.y - state.preview->interaction.drag_start.y;

  SetWindowPos(hwnd, nullptr, rect.left + deltaX, rect.top + deltaY, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER);
}

auto end_window_drag(Core::State::AppState& state, HWND hwnd) -> void {
  state.preview->interaction.is_dragging = false;
  ReleaseCapture();
}

// 视口拖拽实现
auto start_viewport_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void {
  state.preview->interaction.viewport_dragging = true;

  SetCapture(hwnd);
}

auto update_viewport_drag(Core::State::AppState& state, HWND hwnd, POINT pt) -> void {
  if (!state.preview->interaction.viewport_dragging) return;

  RECT clientRect;
  GetClientRect(hwnd, &clientRect);
  float previewWidth = static_cast<float>(clientRect.right - clientRect.left);
  float previewHeight = static_cast<float>(clientRect.bottom - clientRect.top);

  // 计算新的相对位置
  float relativeX =
      static_cast<float>(pt.x) / previewWidth;
  float relativeY =
      static_cast<float>(pt.y) / previewHeight;

  move_game_window_to_position(state, relativeX, relativeY);
}

auto end_viewport_drag(Core::State::AppState& state, HWND hwnd) -> void {
  state.preview->interaction.viewport_dragging = false;
  ReleaseCapture();
}

auto handle_mouse_move(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  if (state.preview->interaction.is_dragging) {
    update_window_drag(state, hwnd, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
  } else if (state.preview->interaction.viewport_dragging) {
    update_viewport_drag(state, hwnd, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
  }
  return 0;
}

auto handle_left_button_down(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

  // 检查是否点击在标题栏
  if (is_point_in_title_bar(state, pt)) {
    start_window_drag(state, hwnd, pt);
    return 0;
  }

  // 如果游戏窗口完全可见，整个预览窗口都可以拖拽
  if (state.preview->viewport.game_window_fully_visible) {
    start_window_drag(state, hwnd, pt);
    return 0;
  }

  // 检查是否点击在视口上，或开始视口拖拽
  bool isOnViewport = is_point_in_viewport(state, pt);

  if (!isOnViewport) {
    // 点击在视口外，先移动游戏窗口到点击位置
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    float previewWidth = static_cast<float>(clientRect.right - clientRect.left);
    float previewHeight = static_cast<float>(clientRect.bottom - clientRect.top);

    float relativeX = static_cast<float>(pt.x) / previewWidth;
    float relativeY = static_cast<float>(pt.y) / previewHeight;

    move_game_window_to_position(state, relativeX, relativeY);
  }

  // 开始视口拖拽
  start_viewport_drag(state, hwnd, pt);
  return 0;
}

auto handle_left_button_up(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  if (state.preview->interaction.is_dragging) {
    end_window_drag(state, hwnd);
  } else if (state.preview->interaction.viewport_dragging) {
    end_viewport_drag(state, hwnd);
  }
  return 0;
}

// 窗口缩放实现
auto handle_window_scaling(Core::State::AppState& state, HWND hwnd, int wheel_delta,
                           POINT mouse_pos) -> void {
  // 计算新的理想尺寸（每次改变10%）
  int oldIdealSize = state.preview->size.ideal_size;
  int newIdealSize = static_cast<int>(oldIdealSize * (1.0f + (wheel_delta > 0 ? 0.1f : -0.1f)));

  // 限制在最小最大范围内
  newIdealSize = std::clamp(newIdealSize, state.preview->size.min_ideal_size,
                            state.preview->size.max_ideal_size);

  if (newIdealSize != oldIdealSize) {
    state.preview->size.ideal_size = newIdealSize;

    // 根据宽高比计算实际窗口尺寸
    int newWidth, newHeight;
    if (state.preview->size.aspect_ratio >= 1.0f) {
      newHeight = newIdealSize;
      newWidth = static_cast<int>(newHeight / state.preview->size.aspect_ratio);
    } else {
      newWidth = newIdealSize;
      newHeight = static_cast<int>(newWidth * state.preview->size.aspect_ratio);
    }

    // 获取当前窗口位置
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    // 计算鼠标相对位置
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    float relativeX = static_cast<float>(mouse_pos.x) / (clientRect.right - clientRect.left);
    float relativeY = static_cast<float>(mouse_pos.y) / (clientRect.bottom - clientRect.top);

    // 计算新位置（保持鼠标指向的点不变）
    int deltaWidth = newWidth - (windowRect.right - windowRect.left);
    int deltaHeight = newHeight - (windowRect.bottom - windowRect.top);
    int newX = windowRect.left - static_cast<int>(deltaWidth * relativeX);
    int newY = windowRect.top - static_cast<int>(deltaHeight * relativeY);

    // 更新窗口
    SetWindowPos(hwnd, nullptr, newX, newY, newWidth, newHeight, SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

auto handle_mouse_wheel(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  // 检查鼠标是否在标题栏
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
  ScreenToClient(hwnd, &pt);

  if (is_point_in_title_bar(state, pt)) {
    return 0;  // 标题栏不处理缩放
  }

  int delta = GET_WHEEL_DELTA_WPARAM(wParam);
  handle_window_scaling(state, hwnd, delta, pt);
  return 0;
}

auto handle_sizing(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  RECT* rect = (RECT*)lParam;
  int width = rect->right - rect->left;
  int height = rect->bottom - rect->top;

  // 根据拖动方向调整大小，保持宽高比
  switch (wParam) {
    case WMSZ_LEFT:
    case WMSZ_RIGHT:
      // 调整宽度，相应调整高度
      width = std::max(width, state.preview->size.min_ideal_size);
      height = static_cast<int>(width * state.preview->size.aspect_ratio);
      if (wParam == WMSZ_LEFT) {
        rect->left = rect->right - width;
      } else {
        rect->right = rect->left + width;
      }
      rect->bottom = rect->top + height;
      break;

    case WMSZ_TOP:
    case WMSZ_BOTTOM:
      // 调整高度，相应调整宽度
      height = std::max(height, state.preview->size.min_ideal_size);
      width = static_cast<int>(height / state.preview->size.aspect_ratio);
      if (wParam == WMSZ_TOP) {
        rect->top = rect->bottom - height;
      } else {
        rect->bottom = rect->top + height;
      }
      rect->right = rect->left + width;
      break;

    case WMSZ_TOPLEFT:
    case WMSZ_TOPRIGHT:
    case WMSZ_BOTTOMLEFT:
    case WMSZ_BOTTOMRIGHT:
      // 对角调整，以宽度为准
      width = std::max(width, state.preview->size.min_ideal_size);
      height = static_cast<int>(width * state.preview->size.aspect_ratio);

      if (wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
        rect->left = rect->right - width;
      } else {
        rect->right = rect->left + width;
      }

      if (wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT) {
        rect->top = rect->bottom - height;
      } else {
        rect->bottom = rect->top + height;
      }
      break;
  }

  // 更新理想尺寸
  state.preview->size.ideal_size = std::max(width, height);
  return TRUE;
}

auto handle_size(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam) -> LRESULT {
  if (!state.preview->rendering_resources.initialized) {
    return 0;
  }

  RECT clientRect;
  GetClientRect(hwnd, &clientRect);
  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  // 更新理想尺寸
  state.preview->size.ideal_size = std::max(width, height);
  state.preview->size.window_width = width;
  state.preview->size.window_height = height;

  // 调整渲染系统大小
  if (auto result = Features::Preview::Rendering::resize_rendering(state, width, height); !result) {
    Logger().error("Failed to resize preview rendering: {}", result.error());
  }

  return 0;
}

auto handle_dpi_changed(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  // 更新DPI
  UINT newDpi = HIWORD(wParam);
  state.preview->dpi_sizes.update_dpi_scaling(newDpi);

  // 使用系统建议的新窗口位置
  RECT* const prcNewWindow = (RECT*)lParam;
  SetWindowPos(hwnd, nullptr, prcNewWindow->left, prcNewWindow->top,
               prcNewWindow->right - prcNewWindow->left, prcNewWindow->bottom - prcNewWindow->top,
               SWP_NOZORDER | SWP_NOACTIVATE);
  return 0;
}

auto handle_nc_hit_test(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
  ScreenToClient(hwnd, &pt);

  // 检查标题栏区域
  if (is_point_in_title_bar(state, pt)) {
    return HTCAPTION;
  }

  // 检查边框区域
  return get_border_hit_test(state, hwnd, pt);
}

auto handle_right_button_up(Core::State::AppState& state, HWND hwnd, WPARAM wParam, LPARAM lParam)
    -> LRESULT {
  if (state.preview->main_window) {
    PostMessage(state.preview->main_window, Core::Constants::WM_PREVIEW_RCLICK, 0, 0);
  }
  return 0;
}

auto handle_paint(Core::State::AppState& state, HWND hwnd) -> LRESULT {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);

  // 获取窗口客户区大小
  RECT rc;
  GetClientRect(hwnd, &rc);

  // 绘制标题栏背景
  RECT titleRect = {0, 0, rc.right, state.preview->dpi_sizes.title_height};
  HBRUSH titleBrush = CreateSolidBrush(RGB(240, 240, 240));
  FillRect(hdc, &titleRect, titleBrush);
  DeleteObject(titleBrush);

  // 绘制标题文本
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(51, 51, 51));
  HFONT hFont = CreateFont(-state.preview->dpi_sizes.font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                           FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Microsoft YaHei"));
  HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

  titleRect.left += state.preview->dpi_sizes.font_size;
  DrawTextW(hdc, L"Preview", -1, &titleRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

  SelectObject(hdc, oldFont);
  DeleteObject(hFont);

  // 绘制分隔线
  RECT sepRect = {0, state.preview->dpi_sizes.title_height - 1, rc.right,
                  state.preview->dpi_sizes.title_height};
  HBRUSH sepBrush = CreateSolidBrush(RGB(229, 229, 229));
  FillRect(hdc, &sepRect, sepBrush);
  DeleteObject(sepBrush);

  EndPaint(hwnd, &ps);
  return 0;
}

auto handle_preview_message(Core::State::AppState& state, HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) -> std::pair<bool, LRESULT> {
  switch (message) {
    case WM_PAINT:
      return {true, handle_paint(state, hwnd)};

    case WM_MOUSEMOVE:
      return {true, handle_mouse_move(state, hwnd, wParam, lParam)};

    case WM_LBUTTONDOWN:
      return {true, handle_left_button_down(state, hwnd, wParam, lParam)};

    case WM_LBUTTONUP:
      return {true, handle_left_button_up(state, hwnd, wParam, lParam)};

    case WM_MOUSEWHEEL:
      return {true, handle_mouse_wheel(state, hwnd, wParam, lParam)};

    case WM_SIZING:
      return {true, handle_sizing(state, hwnd, wParam, lParam)};

    case WM_SIZE:
      return {true, handle_size(state, hwnd, wParam, lParam)};

    case WM_DPICHANGED:
      return {true, handle_dpi_changed(state, hwnd, wParam, lParam)};

    case WM_NCHITTEST:
      return {true, handle_nc_hit_test(state, hwnd, wParam, lParam)};

    case WM_RBUTTONUP:
      return {true, handle_right_button_up(state, hwnd, wParam, lParam)};

    case WM_DESTROY:
      // 清理资源但不调用PostQuitMessage
      return {true, 0};

    default:
      return {false, 0};
  }
}

}  // namespace Features::Preview::Interaction