module;

#include <d2d1.h>
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
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
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
  // 移除 WS_EX_NOACTIVATE 标志，让菜单能够获得焦点以便正确处理焦点丢失事件
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

// 初始化菜单项数据
auto initialize_menu_items(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;
  auto& items = tray_menu.items;

  items.clear();

  const auto& strings = *state.app_window.data.strings;

  // 窗口选择 - 使用第一个窗口ID作为占位符
  items.emplace_back(strings.SELECT_WINDOW, Core::Constants::ID_WINDOW_BASE);

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

  // 分隔线
  items.emplace_back(UI::TrayMenu::State::MenuItem::separator());

  // 退出
  items.emplace_back(strings.EXIT, Core::Constants::ID_EXIT);
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

  // 创建D2D渲染目标
  if (!UI::TrayMenu::D2DContext::initialize_d2d(state, state.tray_menu.hwnd)) {
    return std::unexpected("Failed to initialize D2D for tray menu");
  }

  return {};
}

auto cleanup(Core::State::AppState& state) -> void {
  auto& tray_menu = state.tray_menu;

  // 清理D2D资源
  UI::TrayMenu::D2DContext::cleanup_d2d(state);

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
  // 这样当用户点击菜单外的区域时，菜单会失去焦点并自动关闭
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

  if (tray_menu.hwnd && tray_menu.is_visible) {
    ShowWindow(tray_menu.hwnd, SW_HIDE);
    tray_menu.is_visible = false;
    tray_menu.interaction.hover_index = -1;
  }
}

auto is_menu_visible(const Core::State::AppState& state) -> bool {
  return state.tray_menu.is_visible;
}

auto handle_menu_command(Core::State::AppState& state, int command_id) -> void {
  // 隐藏菜单
  hide_menu(state);

  // 转发命令到主窗口的消息处理器
  // 这样可以复用现有的命令处理逻辑
  if (state.app_window.window.hwnd) {
    PostMessageW(state.app_window.window.hwnd, WM_COMMAND, command_id, 0);
  }
}

}  // namespace UI::TrayMenu
