module;

#include <iostream>
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <strsafe.h>
#include <windowsx.h>

module UI.AppWindow;

import std;
import Core.Constants;

// AppWindow 实现
AppWindow::AppWindow(HINSTANCE hInstance) : m_hInstance(hInstance) {
  RegisterWindowClass();

  // 获取系统 DPI
  if (HDC hdc = GetDC(nullptr); hdc) {
    m_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
  }

  UpdateDpiDependentResources();
}

auto AppWindow::RegisterWindowClass() -> void {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = AppWindowProc;
  wc.hInstance = m_hInstance;
  wc.lpszClassName = L"SpinningMomoAppWindowClass";
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);
}

auto AppWindow::Create(std::span<const AspectRatio> ratios,
                       std::span<const ResolutionPreset> resolutions,
                       const LocalizedStrings& strings, size_t currentRatioIndex,
                       size_t currentResolutionIndex, bool previewEnabled, bool overlayEnabled,
                       bool letterboxEnabled) -> std::expected<void, std::wstring> {
  m_ratioItems = ratios;
  m_resolutionItems = resolutions;
  m_strings = &strings;
  m_currentRatioIndex = currentRatioIndex;
  m_currentResolutionIndex = currentResolutionIndex;
  m_previewEnabled = previewEnabled;
  m_overlayEnabled = overlayEnabled;
  m_letterboxEnabled = letterboxEnabled;

  InitializeItems(strings);

  // 计算总窗口宽度（三列之和）
  const int totalWidth = m_ratioColumnWidth + m_resolutionColumnWidth + m_settingsColumnWidth;
  const int windowHeight = CalculateWindowHeight();

  // 获取主显示器工作区
  RECT workArea{};
  if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0)) {
    return std::unexpected(L"Failed to get work area");
  }

  // 计算窗口位置（屏幕中央）
  const int xPos = (workArea.right - workArea.left - totalWidth) / 2;
  const int yPos = (workArea.bottom - workArea.top - windowHeight) / 2;

  m_hwnd =
      CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                      L"SpinningMomoAppWindowClass", L"SpinningMomo", WS_POPUP | WS_CLIPCHILDREN,
                      xPos, yPos, totalWidth, windowHeight, nullptr, nullptr, m_hInstance, this);

  if (!m_hwnd) {
    return std::unexpected(L"Failed to create window");
  }

  // 设置窗口透明度（204 约为 80% 不透明）
  SetLayeredWindowAttributes(m_hwnd, 0, 204, LWA_ALPHA);

  // 设置窗口圆角和阴影
  MARGINS margins{1, 1, 1, 1};
  DwmExtendFrameIntoClientArea(m_hwnd, &margins);

  DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
  DwmSetWindowAttribute(m_hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));

  BOOL value = TRUE;
  DwmSetWindowAttribute(m_hwnd, DWMWA_ALLOW_NCPAINT, &value, sizeof(value));

  // Windows 11 风格的圆角
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  return {};
}

auto AppWindow::Show() -> void {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_SHOWNA);
    UpdateWindow(m_hwnd);
  }
}

auto AppWindow::Hide() -> void {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_HIDE);
  }
}

auto AppWindow::IsVisible() const -> bool { return m_hwnd && IsWindowVisible(m_hwnd); }

auto AppWindow::ToggleVisibility() -> void {
  if (IsVisible()) {
    Hide();
  } else {
    Show();
  }
}

auto AppWindow::SetCurrentRatio(size_t index) -> void {
  m_currentRatioIndex = index;
  if (m_hwnd) {
    InvalidateRect(m_hwnd, nullptr, TRUE);
  }
}

auto AppWindow::SetCurrentResolution(size_t index) -> void {
  if (index < m_resolutionItems.size()) {
    m_currentResolutionIndex = index;
    if (m_hwnd) {
      InvalidateRect(m_hwnd, nullptr, TRUE);
    }
  }
}

auto AppWindow::SetPreviewEnabled(bool enabled) -> void {
  m_previewEnabled = enabled;
  if (m_hwnd) {
    InvalidateRect(m_hwnd, nullptr, TRUE);
  }
}

auto AppWindow::SetOverlayEnabled(bool enabled) -> void {
  m_overlayEnabled = enabled;
  if (m_hwnd) {
    InvalidateRect(m_hwnd, nullptr, TRUE);
  }
}

auto AppWindow::SetLetterboxEnabled(bool enabled) -> void {
  m_letterboxEnabled = enabled;
  if (m_hwnd) {
    InvalidateRect(m_hwnd, nullptr, TRUE);
  }
}

auto AppWindow::SetMenuItemsToShow(std::span<const std::wstring> items) -> void {
  m_menuItemsToShow.assign(items.begin(), items.end());
}

auto AppWindow::UpdateMenuItems(const LocalizedStrings& strings, bool forceRedraw) -> void {
  m_items.clear();
  InitializeItems(strings);
  if (m_hwnd && forceRedraw) {
    InvalidateRect(m_hwnd, nullptr, TRUE);
  }
}

auto AppWindow::GetHwnd() const -> HWND { return m_hwnd; }

auto AppWindow::Activate() -> void {
  if (m_hwnd) {
    SetForegroundWindow(m_hwnd);
  }
}

auto AppWindow::RegisterHotkey(UINT modifiers, UINT key) -> bool {
    if (m_hwnd) {
        if (::RegisterHotKey(m_hwnd, m_hotkeyId, modifiers, key)) {
            return true;
        }
    }
    return false;
}

auto AppWindow::UnregisterHotkey() -> void {
    if (m_hwnd) {
        ::UnregisterHotKey(m_hwnd, m_hotkeyId);
    }
}

auto AppWindow::InitializeItems(const LocalizedStrings& strings) -> void {
  m_items.clear();

  // 添加比例选项
  for (size_t i = 0; i < m_ratioItems.size(); ++i) {
    m_items.push_back({m_ratioItems[i].name, ItemType::Ratio, static_cast<int>(i)});
  }

  // 添加分辨率选项
  for (size_t i = 0; i < m_resolutionItems.size(); ++i) {
    std::wstring displayText;
    const auto& preset = m_resolutionItems[i];
    if (preset.baseWidth == 0 && preset.baseHeight == 0) {
      displayText = preset.name;
    } else {
      const double megaPixels = preset.totalPixels / 1000000.0;
      wchar_t buffer[16];
      if (megaPixels < 10) {
        swprintf(buffer, 16, L"%.1f", megaPixels);
      } else {
        swprintf(buffer, 16, L"%.0f", megaPixels);
      }
      displayText = preset.name + L" (" + buffer + L"M)";
    }
    m_items.push_back({displayText, ItemType::Resolution, static_cast<int>(i)});
  }

  // 添加设置选项（第三列，根据配置显示）
  if (!m_menuItemsToShow.empty()) {
    for (const auto& itemType : m_menuItemsToShow) {
      if (itemType == Constants::MENU_ITEM_TYPE_CAPTURE) {
        m_items.push_back({strings.CAPTURE_WINDOW, ItemType::CaptureWindow, 0});
      } else if (itemType == Constants::MENU_ITEM_TYPE_SCREENSHOT) {
        m_items.push_back({strings.OPEN_SCREENSHOT, ItemType::OpenScreenshot, 0});
      } else if (itemType == Constants::MENU_ITEM_TYPE_PREVIEW) {
        m_items.push_back({strings.PREVIEW_WINDOW, ItemType::PreviewWindow, 0});
      } else if (itemType == Constants::MENU_ITEM_TYPE_OVERLAY) {
        m_items.push_back({strings.OVERLAY_WINDOW, ItemType::OverlayWindow, 0});
      } else if (itemType == Constants::MENU_ITEM_TYPE_LETTERBOX) {
        m_items.push_back({strings.LETTERBOX_WINDOW, ItemType::LetterboxWindow, 0});
      } else if (itemType == Constants::MENU_ITEM_TYPE_RESET) {
        m_items.push_back({strings.RESET_WINDOW, ItemType::Reset, 0});
      } else if (itemType == Constants::MENU_ITEM_TYPE_CLOSE) {
        m_items.push_back({strings.CLOSE_WINDOW, ItemType::Close, 0});
      } else if (itemType == Constants::MENU_ITEM_TYPE_EXIT) {
        m_items.push_back({strings.EXIT, ItemType::Exit, 0});
      }
    }
  } else {
    // 如果配置为空，则使用默认顺序添加所有菜单项
    m_items.push_back({strings.CAPTURE_WINDOW, ItemType::CaptureWindow, 0});
    m_items.push_back({strings.OPEN_SCREENSHOT, ItemType::OpenScreenshot, 0});
    m_items.push_back({strings.PREVIEW_WINDOW, ItemType::PreviewWindow, 0});
    m_items.push_back({strings.OVERLAY_WINDOW, ItemType::OverlayWindow, 0});
    m_items.push_back({strings.LETTERBOX_WINDOW, ItemType::LetterboxWindow, 0});
    m_items.push_back({strings.RESET_WINDOW, ItemType::Reset, 0});
    m_items.push_back({strings.CLOSE_WINDOW, ItemType::Close, 0});
  }
}

LRESULT CALLBACK AppWindow::AppWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  AppWindow* window = nullptr;

  if (msg == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
    window = reinterpret_cast<AppWindow*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
  } else {
    window = reinterpret_cast<AppWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (window) {
    return window->HandleMessage(hwnd, msg, wParam, lParam);
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

auto AppWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
  switch (msg) {
    case WM_HOTKEY:
        if (wParam == m_hotkeyId) {
            ToggleVisibility();
        }
        return 0;

    case WM_DPICHANGED: {
      m_dpi = HIWORD(wParam);
      UpdateDpiDependentResources();

      const int totalWidth = m_ratioColumnWidth + m_resolutionColumnWidth + m_settingsColumnWidth;
      const int windowHeight = CalculateWindowHeight();

      RECT currentRect{};
      GetWindowRect(hwnd, &currentRect);

      SetWindowPos(hwnd, nullptr, currentRect.left, currentRect.top, totalWidth, windowHeight,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps{};
      if (HDC hdc = BeginPaint(hwnd, &ps); hdc) {
        OnPaint(hdc);
        EndPaint(hwnd, &ps);
      }
      return 0;
    }

    case WM_MOUSEMOVE: {
      OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      return 0;
    }

    case WM_MOUSELEAVE: {
      OnMouseLeave();
      return 0;
    }

    case WM_LBUTTONDOWN: {
      OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      return 0;
    }

    case WM_NCHITTEST: {
      POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      ScreenToClient(hwnd, &pt);
      if (pt.y < m_titleHeight) {
        return HTCAPTION;
      }
      return HTCLIENT;
    }

    case WM_CLOSE:
      Hide();
      return 0;

    case WM_DESTROY:
      UnregisterHotkey();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

auto AppWindow::OnPaint(HDC hdc) -> void {
  RECT rect{};
  GetClientRect(m_hwnd, &rect);

  // 创建双缓冲
  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
  HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, memBitmap));

  // 设置文本属性，使用 DPI 感知的字体大小
  SetBkMode(memDC, TRANSPARENT);
  HFONT hFont = CreateFontW(-m_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
  HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, hFont));

  // 绘制背景
  HBRUSH hBackBrush = CreateSolidBrush(RGB(255, 255, 255));
  FillRect(memDC, &rect, hBackBrush);
  DeleteObject(hBackBrush);

  // 绘制标题栏
  RECT titleRect = rect;
  titleRect.bottom = m_titleHeight;
  HBRUSH hTitleBrush = CreateSolidBrush(RGB(240, 240, 240));
  FillRect(memDC, &titleRect, hTitleBrush);
  DeleteObject(hTitleBrush);

  // 绘制标题文本
  SetTextColor(memDC, RGB(51, 51, 51));
  titleRect.left += m_textPadding;
  DrawTextW(memDC, L"SpinningMomo", -1, &titleRect,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOCLIP);

  // 绘制分隔线
  RECT sepRect{rect.left, m_titleHeight, rect.right, m_titleHeight + m_separatorHeight};
  HBRUSH hSepBrush = CreateSolidBrush(RGB(229, 229, 229));
  FillRect(memDC, &sepRect, hSepBrush);

  // 计算列宽和位置
  const int ratioColumnRight = m_ratioColumnWidth;
  const int resolutionColumnRight = ratioColumnRight + m_resolutionColumnWidth;

  // 绘制垂直分隔线
  RECT vSepRect1{ratioColumnRight, m_titleHeight, ratioColumnRight + m_separatorHeight,
                 rect.bottom};
  RECT vSepRect2{resolutionColumnRight, m_titleHeight, resolutionColumnRight + m_separatorHeight,
                 rect.bottom};
  FillRect(memDC, &vSepRect1, hSepBrush);
  FillRect(memDC, &vSepRect2, hSepBrush);
  DeleteObject(hSepBrush);

  // 绘制列表项
  int y = m_titleHeight + m_separatorHeight;
  int settingsY = y;

  for (size_t i = 0; i < m_items.size(); ++i) {
    const auto& item = m_items[i];
    RECT itemRect{};

    // 根据项目类型确定绘制位置
    switch (item.type) {
      case ItemType::Ratio:
        itemRect = {0, y, ratioColumnRight, y + m_itemHeight};
        break;
      case ItemType::Resolution:
        itemRect = {ratioColumnRight + m_separatorHeight, y, resolutionColumnRight,
                    y + m_itemHeight};
        break;
      case ItemType::CaptureWindow:
      case ItemType::OpenScreenshot:
      case ItemType::PreviewWindow:
      case ItemType::OverlayWindow:
      case ItemType::LetterboxWindow:
      case ItemType::Reset:
      case ItemType::Close:
      case ItemType::Exit:
        itemRect = {resolutionColumnRight + m_separatorHeight, settingsY, rect.right,
                    settingsY + m_itemHeight};
        settingsY += m_itemHeight;
        break;
      default:
        continue;
    }

    // 绘制悬停背景
    if (static_cast<int>(i) == m_hoverIndex) {
      HBRUSH hHoverBrush = CreateSolidBrush(RGB(242, 242, 242));
      FillRect(memDC, &itemRect, hHoverBrush);
      DeleteObject(hHoverBrush);
    }

    // 绘制选中指示器
    bool isSelected = false;
    if (item.type == ItemType::Ratio && item.index == static_cast<int>(m_currentRatioIndex)) {
      isSelected = true;
    } else if (item.type == ItemType::Resolution &&
               item.index == static_cast<int>(m_currentResolutionIndex)) {
      isSelected = true;
    } else if (item.type == ItemType::OverlayWindow && m_overlayEnabled) {
      isSelected = true;
    } else if (item.type == ItemType::PreviewWindow && m_previewEnabled) {
      isSelected = true;
    } else if (item.type == ItemType::LetterboxWindow && m_letterboxEnabled) {
      isSelected = true;
    }

    if (isSelected) {
      const int indicatorWidth =
          (item.type == ItemType::Ratio) ? m_ratioIndicatorWidth : m_indicatorWidth;
      RECT indicatorRect{itemRect.left, itemRect.top, itemRect.left + indicatorWidth,
                         itemRect.bottom};
      HBRUSH hIndicatorBrush = CreateSolidBrush(RGB(255, 160, 80));
      FillRect(memDC, &indicatorRect, hIndicatorBrush);
      DeleteObject(hIndicatorBrush);
    }

    // 绘制文本
    itemRect.left +=
        m_textPadding + ((item.type == ItemType::Ratio) ? m_ratioIndicatorWidth : m_indicatorWidth);
    SetTextColor(memDC, RGB(51, 51, 51));
    DrawTextW(memDC, item.text.c_str(), -1, &itemRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // 只有在同一列中才增加y坐标
    if ((i + 1 < m_items.size()) && (m_items[i + 1].type == item.type)) {
      if (item.type != ItemType::Reset) {
        y += m_itemHeight;
      }
    } else if (i + 1 < m_items.size() && m_items[i + 1].type != item.type) {
      if (m_items[i + 1].type != ItemType::Reset) {
        y = m_titleHeight + m_separatorHeight;
      }
    }
  }

  // 复制到屏幕
  BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

  // 清理
  SelectObject(memDC, oldFont);
  DeleteObject(hFont);
  SelectObject(memDC, oldBitmap);
  DeleteObject(memBitmap);
  DeleteDC(memDC);
}

auto AppWindow::OnMouseMove(int x, int y) -> void {
  const int newHoverIndex = GetItemIndexFromPoint(x, y);
  if (newHoverIndex != m_hoverIndex) {
    m_hoverIndex = newHoverIndex;
    InvalidateRect(m_hwnd, nullptr, TRUE);

    // 确保能收到 WM_MOUSELEAVE 消息
    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hwnd;
    TrackMouseEvent(&tme);
  }
}

auto AppWindow::OnMouseLeave() -> void {
  if (m_hoverIndex != -1) {
    m_hoverIndex = -1;
    InvalidateRect(m_hwnd, nullptr, TRUE);
  }
}

auto AppWindow::OnLButtonDown(int x, int y) -> void {
  if (m_hoverIndex >= 0 && m_hoverIndex < static_cast<int>(m_items.size())) {
    const auto& item = m_items[m_hoverIndex];

    // 不再需要发送消息到父窗口，直接处理或通过回调
    // SetForegroundWindow(m_hwndParent);

    switch (item.type) {
      case ItemType::Ratio:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_RATIO_BASE + item.index, 0);
        SetCurrentRatio(item.index);
        break;
      case ItemType::Resolution:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_RESOLUTION_BASE + item.index, 0);
        SetCurrentResolution(item.index);
        break;
      case ItemType::CaptureWindow:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_CAPTURE_WINDOW, 0);
        break;
      case ItemType::OpenScreenshot:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_OPEN_SCREENSHOT, 0);
        break;
      case ItemType::OverlayWindow:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_OVERLAY_WINDOW, 0);
        SetOverlayEnabled(!m_overlayEnabled);
        break;
      case ItemType::LetterboxWindow:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_LETTERBOX_WINDOW, 0);
        SetLetterboxEnabled(!m_letterboxEnabled);
        break;
      case ItemType::PreviewWindow:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_PREVIEW_WINDOW, 0);
        SetPreviewEnabled(!m_previewEnabled);
        break;
      case ItemType::Reset:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_RESET, 0);
        break;
      case ItemType::Close:
        Hide();
        break;
      case ItemType::Exit:
        // SendMessage(m_hwndParent, WM_COMMAND, Constants::ID_EXIT, 0);
        DestroyWindow(m_hwnd);
        break;
    }
    // 隐藏窗口以表示操作完成
    Hide();
  }
}

auto AppWindow::UpdateDpiDependentResources() -> void {
  const double scale = static_cast<double>(m_dpi) / 96.0;
  m_itemHeight = static_cast<int>(BASE_ITEM_HEIGHT * scale);
  m_titleHeight = static_cast<int>(BASE_TITLE_HEIGHT * scale);
  m_separatorHeight = static_cast<int>(BASE_SEPARATOR_HEIGHT * scale);
  m_fontSize = static_cast<int>(BASE_FONT_SIZE * scale);
  m_textPadding = static_cast<int>(BASE_TEXT_PADDING * scale);
  m_indicatorWidth = static_cast<int>(BASE_INDICATOR_WIDTH * scale);
  m_ratioIndicatorWidth = static_cast<int>(BASE_RATIO_INDICATOR_WIDTH * scale);
  m_ratioColumnWidth = static_cast<int>(BASE_RATIO_COLUMN_WIDTH * scale);
  m_resolutionColumnWidth = static_cast<int>(BASE_RESOLUTION_COLUMN_WIDTH * scale);
  m_settingsColumnWidth = static_cast<int>(BASE_SETTINGS_COLUMN_WIDTH * scale);
}

auto AppWindow::CalculateWindowHeight() const -> int {
  // 计算每列的项目数量
  int ratioCount = 0;
  int resolutionCount = 0;
  int settingsCount = 0;

  for (const auto& item : m_items) {
    switch (item.type) {
      case ItemType::Ratio:
        ++ratioCount;
        break;
      case ItemType::Resolution:
        ++resolutionCount;
        break;
      case ItemType::CaptureWindow:
      case ItemType::OpenScreenshot:
      case ItemType::OverlayWindow:
      case ItemType::LetterboxWindow:
      case ItemType::PreviewWindow:
      case ItemType::Reset:
      case ItemType::Close:
      case ItemType::Exit:
        ++settingsCount;
        break;
    }
  }

  // 计算每列的高度
  const int ratioHeight = ratioCount * m_itemHeight;
  const int resolutionHeight = resolutionCount * m_itemHeight;
  const int settingsHeight = settingsCount * m_itemHeight;

  // 找出最大高度
  const int maxColumnHeight = std::max({ratioHeight, resolutionHeight, settingsHeight});

  // 返回总高度
  return m_titleHeight + m_separatorHeight + maxColumnHeight;
}

auto AppWindow::GetItemIndexFromPoint(int x, int y) const -> int {
  // 检查是否在标题栏或分隔线区域
  if (y < m_titleHeight + m_separatorHeight) return -1;

  // 计算列的边界
  const int ratioColumnRight = m_ratioColumnWidth;
  const int resolutionColumnRight = ratioColumnRight + m_resolutionColumnWidth;

  // 确定点击的是哪一列
  ItemType targetType;
  if (x < ratioColumnRight) {
    targetType = ItemType::Ratio;
  } else if (x < resolutionColumnRight) {
    targetType = ItemType::Resolution;
  } else {
    // 设置列的特殊处理
    int settingsY = m_titleHeight + m_separatorHeight;
    for (size_t i = 0; i < m_items.size(); ++i) {
      const auto& item = m_items[i];
      if (item.type == ItemType::CaptureWindow || item.type == ItemType::OpenScreenshot ||
          item.type == ItemType::PreviewWindow || item.type == ItemType::OverlayWindow ||
          item.type == ItemType::LetterboxWindow || item.type == ItemType::Reset ||
          item.type == ItemType::Close || item.type == ItemType::Exit) {
        if (y >= settingsY && y < settingsY + m_itemHeight) {
          return static_cast<int>(i);
        }
        settingsY += m_itemHeight;
      }
    }
    return -1;
  }

  // 处理比例和分辨率列
  int itemY = m_titleHeight + m_separatorHeight;
  for (size_t i = 0; i < m_items.size(); ++i) {
    const auto& item = m_items[i];
    if (item.type == targetType) {
      if (y >= itemY && y < itemY + m_itemHeight) {
        return static_cast<int>(i);
      }
      itemY += m_itemHeight;
    }
  }

  return -1;
}