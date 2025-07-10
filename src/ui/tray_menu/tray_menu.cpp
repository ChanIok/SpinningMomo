module;

#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include <format>

module UI.TrayMenu;

import std;
import Core.State;
import Core.Constants;
import Types.UI;
import UI.TrayMenu.State;
import UI.TrayMenu.Layout;
import UI.TrayMenu.MessageHandler;
import UI.TrayMenu.Painter;
import UI.TrayMenu.D2DContext;
import Utils.Logger;
import Vendor.Windows;

namespace {

// 窗口类名
constexpr const wchar_t* TRAY_MENU_CLASS_NAME = L"SpinningMomoTrayMenuClass";

// 注册窗口类
auto register_tray_menu_class(HINSTANCE instance) -> bool {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = UI::TrayMenu::MessageHandler::static_window_proc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = sizeof(Core::State::AppState*);
  wc.hInstance = instance;
  wc.hIcon = nullptr;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;  // 我们使用D2D绘制，不需要背景画刷
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = TRAY_MENU_CLASS_NAME;
  wc.hIconSm = nullptr;

  return RegisterClassExW(&wc) != 0;
}

// 创建托盘菜单窗口
auto create_tray_menu_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& tray_menu = state.tray_menu;

  if (tray_menu.is_created) {
    return {};
  }

  // 注册窗口类
  if (!register_tray_menu_class(state.app_window.window.instance)) {
    DWORD error = GetLastError();
    if (error != ERROR_CLASS_ALREADY_EXISTS) {
      return std::unexpected(
          std::format("Failed to register tray menu window class, error: {}", error));
    }
  }

  // 创建窗口（初始时隐藏）
  tray_menu.hwnd =
      CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, TRAY_MENU_CLASS_NAME,
                      L"TrayMenu", WS_POPUP, 0, 0, 100, 100,  // 临时尺寸，稍后会调整
                      nullptr, nullptr, state.app_window.window.instance, &state);

  if (!tray_menu.hwnd) {
    return std::unexpected(
        std::format("Failed to create tray menu window, error: {}", GetLastError()));
  }

  // 设置窗口透明度
  SetLayeredWindowAttributes(tray_menu.hwnd, 0, 240, LWA_ALPHA);

  tray_menu.is_created = true;
  return {};
}

// 构建窗口列表子菜单
auto build_window_list(Core::State::AppState& state) -> std::vector<UI::TrayMenu::State::MenuItem> {
  std::vector<UI::TrayMenu::State::MenuItem> windows;

  // 枚举所有可见窗口
  EnumWindows(
      [](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* windows = reinterpret_cast<std::vector<UI::TrayMenu::State::MenuItem>*>(lParam);

        // 检查窗口是否可见且不是最小化状态
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
          wchar_t title[256];
          if (GetWindowTextW(hwnd, title, 256) > 0) {
            // 过滤掉一些系统窗口和不需要的窗口
            std::wstring window_title(title);
            if (!window_title.empty() && window_title != L"Program Manager" &&
                window_title.find(L"SpinningMomo") == std::wstring::npos) {
              // 为每个窗口分配唯一的命令ID
              int id = Core::Constants::ID_WINDOW_BASE + static_cast<int>(windows->size()) + 1;
              windows->emplace_back(window_title, id);
            }
          }
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&windows));

  // 如果没有找到窗口，添加一个提示项
  if (windows.empty()) {
    windows.emplace_back(L"(无可用窗口)", 0);
    windows.back().is_enabled = false;
  }

  return windows;
}

// 初始化菜单项数据
auto initialize_menu_items(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;
  auto& items = tray_menu.items;

  items.clear();

  const auto& strings = *state.app_window.data.strings;

  // 窗口选择 - 构建子菜单
  UI::TrayMenu::State::MenuItem window_item(strings.SELECT_WINDOW, Core::Constants::ID_WINDOW_BASE);
  window_item.submenu_items = build_window_list(state);
  items.emplace_back(std::move(window_item));

  // 分隔线
  items.emplace_back(UI::TrayMenu::State::MenuItem::separator());

  // 窗口设置 - 使用比例和分辨率的基础ID
  items.emplace_back(strings.WINDOW_RATIO, Core::Constants::ID_RATIO_BASE);
  items.emplace_back(strings.RESOLUTION, Core::Constants::ID_RESOLUTION_BASE);

  // 分隔线
  items.emplace_back(UI::TrayMenu::State::MenuItem::separator());

  // 功能选项
  items.emplace_back(strings.CAPTURE_WINDOW, Core::Constants::ID_CAPTURE_WINDOW);
  items.emplace_back(strings.PREVIEW_WINDOW, Core::Constants::ID_PREVIEW_WINDOW,
                     state.app_window.ui.preview_enabled);
  items.emplace_back(strings.OVERLAY_WINDOW, Core::Constants::ID_OVERLAY_WINDOW,
                     state.app_window.ui.overlay_enabled);

  // 分隔线
  items.emplace_back(UI::TrayMenu::State::MenuItem::separator());

  // 系统选项
  items.emplace_back(strings.OPEN_CONFIG, Core::Constants::ID_CONFIG);
  items.emplace_back(strings.USER_GUIDE, Core::Constants::ID_USER_GUIDE);
  items.emplace_back(strings.WEBVIEW_TEST, Core::Constants::ID_WEBVIEW_TEST);

  // 分隔线
  items.emplace_back(UI::TrayMenu::State::MenuItem::separator());

  // 退出
  items.emplace_back(strings.EXIT, Core::Constants::ID_EXIT);
}

auto create_window_attributes(HWND hwnd) -> void {
  // 设置窗口样式
  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

  // 设置DWM属性
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

}  // anonymous namespace

namespace UI::TrayMenu {

auto initialize(Core::State::AppState& state) -> std::expected<void, std::string> {
  // 更新DPI缩放
  state.tray_menu.layout.update_dpi_scaling(state.app_window.window.dpi);

  // 初始化菜单项
  initialize_menu_items(state);

  // 创建窗口
  if (auto result = create_tray_menu_window(state); !result) {
    return result;
  }

  create_window_attributes(state.tray_menu.hwnd);

  // 创建主菜单D2D资源
  if (!UI::TrayMenu::D2DContext::initialize_main_menu(state, state.tray_menu.hwnd)) {
    return std::unexpected("Failed to initialize D2D for main menu");
  }

  return {};
}

auto cleanup(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;

  // 清理主菜单D2D资源
  UI::TrayMenu::D2DContext::cleanup_main_menu(state);

  if (tray_menu.hwnd) {
    DestroyWindow(tray_menu.hwnd);
    tray_menu.hwnd = nullptr;
  }

  tray_menu.is_created = false;
  tray_menu.is_visible = false;
}

auto show_menu(Core::State::AppState& state, const Vendor::Windows::POINT& position) -> void {
  auto& tray_menu = state.tray_menu;

  // 如果菜单已经可见，直接返回
  if (tray_menu.is_visible) {
    return;
  }

  if (!tray_menu.is_created) {
    if (auto result = initialize(state); !result) {
      Logger().error("Failed to initialize tray menu: {}", result.error());
      return;
    }
  }

  // 检查D2D是否已初始化
  if (!state.d2d_render.is_initialized) {
    Logger().warn("D2D not initialized, tray menu may not render correctly");
  }

  // 更新菜单项状态
  initialize_menu_items(state);

  // 检查是否有菜单项
  if (tray_menu.items.empty()) {
    Logger().warn("No menu items to display");
    return;
  }

  // 计算菜单尺寸
  UI::TrayMenu::Layout::calculate_menu_size(state);

  // 计算菜单位置（确保在屏幕内）
  tray_menu.position = UI::TrayMenu::Layout::calculate_menu_position(state, position);

  // 设置窗口位置和尺寸
  SetWindowPos(tray_menu.hwnd, HWND_TOPMOST, tray_menu.position.x, tray_menu.position.y,
               tray_menu.menu_size.cx, tray_menu.menu_size.cy, SWP_NOACTIVATE);

  // 显示窗口并激活它以获得焦点
  ShowWindow(tray_menu.hwnd, SW_SHOW);
  SetForegroundWindow(tray_menu.hwnd);
  SetFocus(tray_menu.hwnd);

  // 初始化键盘导航：选中第一个可用的菜单项
  tray_menu.interaction.hover_index = -1;
  for (size_t i = 0; i < tray_menu.items.size(); ++i) {
    if (tray_menu.items[i].type == UI::TrayMenu::State::MenuItemType::Normal &&
        tray_menu.items[i].is_enabled) {
      tray_menu.interaction.hover_index = static_cast<int>(i);
      break;
    }
  }

  // 触发重绘
  InvalidateRect(tray_menu.hwnd, nullptr, FALSE);

  tray_menu.is_visible = true;
}

auto hide_menu(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;
  auto& interaction = tray_menu.interaction;

  // 先隐藏子菜单
  hide_submenu(state);

  if (tray_menu.hwnd && tray_menu.is_visible) {
    // 清理所有定时器
    if (interaction.show_timer_id != 0) {
      KillTimer(tray_menu.hwnd, interaction.show_timer_id);
      interaction.show_timer_id = 0;
      interaction.pending_submenu_index = -1;
    }

    // 清理隐藏定时器
    if (interaction.hide_timer_id != 0) {
      KillTimer(tray_menu.hwnd, interaction.hide_timer_id);
      interaction.hide_timer_id = 0;
    }

    ShowWindow(tray_menu.hwnd, SW_HIDE);
    tray_menu.is_visible = false;
    tray_menu.interaction.hover_index = -1;
  }
}

auto is_menu_visible(const Core::State::AppState& state) -> bool {
  return state.tray_menu.is_visible;
}

auto handle_menu_command(Core::State::AppState& state, int command_id) -> void {
  // 隐藏菜单（包括子菜单）
  hide_menu(state);

  // 转发命令到主窗口的消息处理器
  // 这样可以复用现有的命令处理逻辑
  if (state.app_window.window.hwnd) {
    PostMessageW(state.app_window.window.hwnd, WM_COMMAND, command_id, 0);
  }
}

// 计算子菜单尺寸
auto calculate_submenu_size(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;
  const auto& layout = tray_menu.layout;

  if (tray_menu.current_submenu.empty()) {
    tray_menu.submenu_size = {0, 0};
    return;
  }

  // 计算子菜单的宽度和高度
  int max_width = layout.min_width;
  int total_height = layout.padding * 2;

  for (const auto& item : tray_menu.current_submenu) {
    if (item.type == UI::TrayMenu::State::MenuItemType::Separator) {
      total_height += layout.separator_height;
    } else {
      total_height += layout.item_height;
      // 简单估算文本宽度（实际应该使用文本测量）
      int text_width =
          static_cast<int>(item.text.length() * layout.font_size * 0.6) + layout.text_padding * 2;
      max_width = std::max(max_width, text_width);
    }
  }

  tray_menu.submenu_size = {max_width, total_height};
}

// 计算子菜单位置
auto calculate_submenu_position(Core::State::AppState& state, int parent_index) -> void {
  auto& tray_menu = state.tray_menu;
  const auto& layout = tray_menu.layout;

  // 计算父菜单项的位置
  int parent_y = layout.padding;
  for (int i = 0; i < parent_index; ++i) {
    const auto& item = tray_menu.items[i];
    if (item.type == UI::TrayMenu::State::MenuItemType::Separator) {
      parent_y += layout.separator_height;
    } else {
      parent_y += layout.item_height;
    }
  }

  // 子菜单显示在主菜单右侧
  tray_menu.submenu_position.x = tray_menu.position.x + tray_menu.menu_size.cx;
  tray_menu.submenu_position.y = tray_menu.position.y + parent_y;

  // 确保子菜单不会超出屏幕边界
  RECT screen_rect;
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &screen_rect, 0);

  // 检查右边界
  if (tray_menu.submenu_position.x + tray_menu.submenu_size.cx > screen_rect.right) {
    // 显示在主菜单左侧
    tray_menu.submenu_position.x = tray_menu.position.x - tray_menu.submenu_size.cx;
  }

  // 检查下边界
  if (tray_menu.submenu_position.y + tray_menu.submenu_size.cy > screen_rect.bottom) {
    tray_menu.submenu_position.y = screen_rect.bottom - tray_menu.submenu_size.cy;
  }

  // 检查上边界
  if (tray_menu.submenu_position.y < screen_rect.top) {
    tray_menu.submenu_position.y = screen_rect.top;
  }
}

// 显示子菜单
auto show_submenu(Core::State::AppState& state, int parent_index) -> void {
  auto& tray_menu = state.tray_menu;

  Logger().debug("show_submenu called with parent_index: {}", parent_index);

  // 如果已经显示相同的子菜单，直接返回
  if (tray_menu.submenu_hwnd && tray_menu.submenu_parent_index == parent_index) {
    Logger().debug("Submenu already showing for the same parent, skipping");
    return;
  }

  // 隐藏当前子菜单
  hide_submenu(state);

  if (parent_index < 0 || parent_index >= static_cast<int>(tray_menu.items.size())) {
    Logger().warn("Invalid parent_index: {}", parent_index);
    return;
  }

  const auto& parent_item = tray_menu.items[parent_index];
  if (!parent_item.has_submenu()) {
    Logger().warn("Parent item at index {} has no submenu", parent_index);
    return;
  }

  // 设置子菜单数据
  tray_menu.current_submenu = parent_item.submenu_items;
  tray_menu.submenu_parent_index = parent_index;

  // 计算子菜单尺寸和位置
  calculate_submenu_size(state);
  calculate_submenu_position(state, parent_index);

  Logger().debug("Submenu size: {}x{}, position: ({}, {})", tray_menu.submenu_size.cx,
                 tray_menu.submenu_size.cy, tray_menu.submenu_position.x,
                 tray_menu.submenu_position.y);

  // 创建子菜单窗口
  tray_menu.submenu_hwnd = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, TRAY_MENU_CLASS_NAME, L"TraySubmenu",
      WS_POPUP, tray_menu.submenu_position.x, tray_menu.submenu_position.y,
      tray_menu.submenu_size.cx, tray_menu.submenu_size.cy,
      tray_menu.hwnd,  // 父窗口
      nullptr, state.app_window.window.instance, &state);

  if (tray_menu.submenu_hwnd) {
    Logger().debug("Submenu window created successfully: {}", (void*)tray_menu.submenu_hwnd);

    create_window_attributes(tray_menu.submenu_hwnd);

    // 初始化子菜单D2D资源
    if (!UI::TrayMenu::D2DContext::initialize_submenu(state, tray_menu.submenu_hwnd)) {
      Logger().error("Failed to initialize submenu D2D resources");
      DestroyWindow(tray_menu.submenu_hwnd);
      tray_menu.submenu_hwnd = nullptr;
      return;
    }

    ShowWindow(tray_menu.submenu_hwnd, SW_SHOW);

    // 强制置顶确保可见性
    SetWindowPos(tray_menu.submenu_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  } else {
    Logger().error("Failed to create submenu window, error: {}", GetLastError());
  }
}

// 隐藏子菜单
auto hide_submenu(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;

  if (tray_menu.submenu_hwnd) {
    Logger().debug("Hiding submenu window: {}", (void*)tray_menu.submenu_hwnd);

    // 清理子菜单D2D资源
    UI::TrayMenu::D2DContext::cleanup_submenu(state);

    DestroyWindow(tray_menu.submenu_hwnd);
    tray_menu.submenu_hwnd = nullptr;
    tray_menu.submenu_parent_index = -1;
    tray_menu.current_submenu.clear();
    tray_menu.interaction.submenu_hover_index = -1;  // 重置子菜单悬停索引

  } else {
    Logger().debug("hide_submenu called but no submenu exists");
  }
}

}  // namespace UI::TrayMenu
