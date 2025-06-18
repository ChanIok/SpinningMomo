module;

#include <iostream>
#include <windows.h>

module Features.WindowControl;

import std;

namespace Features::WindowControl {

// 查找目标窗口
auto find_target_window(const std::wstring& configured_title) -> std::expected<HWND, std::string> {
  HWND gameWindow = nullptr;

  // 辅助函数：直接比较窗口标题
  auto compareWindowTitle = [](const std::wstring& title1, const std::wstring& title2) -> bool {
    return title1 == title2;  // 精确比较
  };

  // 1. 如果有配置的标题，先尝试使用配置的标题查找
  if (!configured_title.empty()) {
    auto windows = get_visible_windows();
    for (const auto& window : windows) {
      if (compareWindowTitle(window.title, configured_title)) {
        gameWindow = window.handle;
        break;
      }
    }
  }

  // 2. 如果找不到，尝试预设的游戏窗口标题
  if (!gameWindow) {
    // 先尝试查找中文标题
    gameWindow = FindWindow(nullptr, L"无限暖暖  ");  // 精确匹配，包括两个尾部空格
    if (!gameWindow) {
      // 如果找不到中文标题，尝试英文标题
      gameWindow = FindWindow(nullptr, L"Infinity Nikki  ");
    }
  }

  if (gameWindow) {
    return gameWindow;
  }
  return std::unexpected{"Target window not found. Please ensure the game is running."};
}

// 调整窗口大小并居中
auto resize_and_center_window(HWND hwnd, int width, int height, bool taskbar_lower, bool activate)
    -> std::expected<void, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Failed to resize window: Invalid window handle provided."};
  }

  // 获取窗口样式
  DWORD style = GetWindowLong(hwnd, GWL_STYLE);
  if (style == 0) {
    return std::unexpected{"Failed to get window style (GetWindowLong)."};
  }
  DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

  // 如果是有边框窗口且需要超出屏幕尺寸，转换为无边框
  if ((style & WS_OVERLAPPEDWINDOW) &&
      (width >= GetSystemMetrics(SM_CXSCREEN) || height >= GetSystemMetrics(SM_CYSCREEN))) {
    style &= ~(WS_OVERLAPPEDWINDOW);
    style |= WS_POPUP;
    if (SetWindowLong(hwnd, GWL_STYLE, style) == 0) {
      return std::unexpected{"Failed to remove window border (SetWindowLong)."};
    }
  }
  // 如果是无边框窗口且高度小于屏幕高度，转换为有边框
  else if ((style & WS_POPUP) && width < GetSystemMetrics(SM_CXSCREEN) &&
           height < GetSystemMetrics(SM_CYSCREEN)) {
    style &= ~(WS_POPUP);
    style |= WS_OVERLAPPEDWINDOW;
    if (SetWindowLong(hwnd, GWL_STYLE, style) == 0) {
      return std::unexpected{"Failed to restore window border (SetWindowLong)."};
    }
  }

  // 调整窗口大小
  RECT rect = {0, 0, width, height};
  if (!AdjustWindowRectEx(&rect, style, FALSE, exStyle)) {
    return std::unexpected{"Failed to calculate window rectangle (AdjustWindowRectEx)."};
  }

  // 使用 rect 的 left 和 top 值来调整位置这些值通常是负数
  int totalWidth = rect.right - rect.left;
  int totalHeight = rect.bottom - rect.top;
  int borderOffsetX = rect.left;  // 左边框的偏移量（负值）
  int borderOffsetY = rect.top;   // 顶部边框的偏移量（负值）

  // 计算屏幕中心位置，考虑边框偏移
  int newLeft = (GetSystemMetrics(SM_CXSCREEN) - width) / 2 + borderOffsetX;
  int newTop = (GetSystemMetrics(SM_CYSCREEN) - height) / 2 + borderOffsetY;

  UINT flags = SWP_NOZORDER;
  if (!activate) {
    flags |= SWP_NOACTIVATE;
  }

  // 设置新的窗口大小和位置
  if (!SetWindowPos(hwnd, nullptr, newLeft, newTop, totalWidth, totalHeight, flags)) {
    return std::unexpected{"Failed to set window position and size (SetWindowPos)."};
  }

  // 如果窗口调整成功且需要置底任务栏，则执行置底操作
  if (taskbar_lower) {
    if (HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr)) {
      SetWindowPos(taskbar, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
  }

  return {};
}

// 获取所有可见窗口的列表
auto get_visible_windows() -> std::vector<WindowInfo> {
  std::vector<WindowInfo> windows;

  // 回调函数
  auto enumWindowsProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
    wchar_t className[256];
    wchar_t windowText[256];

    if (!IsWindowVisible(hwnd)) {
      return TRUE;
    }
    if (!GetClassName(hwnd, className, 256)) {
      return TRUE;
    }
    if (!GetWindowText(hwnd, windowText, 256)) {
      return TRUE;
    }

    auto* windows_ptr = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
    if (windowText[0] != '\0') {  // 只收集有标题的窗口
      windows_ptr->push_back({hwnd, windowText});
    }

    return TRUE;
  };

  EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&windows));
  return windows;
}

// 切换窗口边框
auto toggle_window_border(HWND hwnd) -> std::expected<bool, std::string> {
  if (!hwnd || !IsWindow(hwnd)) {
    return std::unexpected{"Failed to toggle window border: Invalid window handle provided."};
  }

  // 获取当前窗口样式
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  if (style == 0) {
    return std::unexpected{"Failed to get window style (GetWindowLong)."};
  }

  // 检查当前是否有边框
  bool hasBorder = (style & WS_OVERLAPPEDWINDOW) != 0;

  if (hasBorder) {
    // 移除边框样式
    style &= ~WS_OVERLAPPEDWINDOW;
    style |= WS_POPUP;  // 添加WS_POPUP样式
  } else {
    // 添加边框样式
    style &= ~WS_POPUP;  // 移除WS_POPUP样式
    style |= WS_OVERLAPPEDWINDOW;
  }

  // 应用新样式
  if (SetWindowLong(hwnd, GWL_STYLE, style) == 0) {
    return std::unexpected{"Failed to apply new window style (SetWindowLong)."};
  }

  // 强制窗口重绘和重新布局
  if (!SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED)) {
    return std::unexpected{"Failed to force window repaint (SetWindowPos)."};
  }

  // 返回切换后的状态
  return !hasBorder;
}

}  // namespace Features::WindowControl