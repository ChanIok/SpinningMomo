module;

#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include <format>

module UI.ContextMenu;

import std;
import Core.State;
import Core.I18n.State;
import Core.I18n.Types;
import Core.Events;
import Common.MenuIds;
import UI.AppWindow.Types;
import UI.AppWindow.State;
import UI.AppWindow.Events;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import UI.ContextMenu.Layout;
import UI.ContextMenu.MessageHandler;
import UI.ContextMenu.Painter;
import UI.ContextMenu.D2DContext;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;
import Features.WindowControl;
import Common.MenuData;

namespace UI::ContextMenu {

auto register_context_menu_class(HINSTANCE instance, WNDPROC wnd_proc) -> bool {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wnd_proc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = instance;
  wc.hIcon = nullptr;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = L"SpinningMomoContextMenuClass";
  wc.hIconSm = nullptr;

  if (!RegisterClassExW(&wc)) {
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  }
  return true;
}

auto create_context_menu_window(HINSTANCE instance, WNDPROC wnd_proc,
                                Core::State::AppState* app_state) -> HWND {
  HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                              L"SpinningMomoContextMenuClass",
                              L"ContextMenu",        // 窗口标题不重要
                              WS_POPUP, 0, 0, 1, 1,  // 初始尺寸很小，稍后调整
                              nullptr, nullptr, instance,
                              app_state  // 将AppState指针作为创建参数传递
  );

  if (hwnd) {
    // 设置窗口样式
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
  }

  return hwnd;
}

// 隐藏并销毁菜单窗口
void hide_and_destroy_menu(Core::State::AppState& state) {
  // 先销毁子菜单
  if (state.context_menu->submenu_hwnd) {
    DestroyWindow(state.context_menu->submenu_hwnd);
    state.context_menu->submenu_hwnd = nullptr;
    // 确保清理子菜单D2D资源
    D2DContext::cleanup_submenu(state);
  }

  // 再销毁主菜单
  if (state.context_menu->hwnd) {
    DestroyWindow(state.context_menu->hwnd);
    state.context_menu->hwnd = nullptr;
    // 确保清理主菜单D2D资源
    D2DContext::cleanup_context_menu(state);
  }
}

// 处理菜单命令
void handle_menu_action(Core::State::AppState& state,
                        const UI::ContextMenu::Types::MenuItem& item) {
  if (!item.has_action()) {
    Logger().warn("Menu item '{}' has no associated action", Utils::String::ToUtf8(item.text));
    return;
  }
  const auto& action = item.action.value();

  // 根据动作类型发送相应的事件
  switch (action.type) {
    case UI::ContextMenu::Types::MenuAction::Type::WindowSelection: {
      try {
        auto window_info = std::any_cast<Features::WindowControl::WindowInfo>(action.data);
        // 使用新的事件系统发送窗口选择事件
        Core::Events::send(*state.events, UI::AppWindow::Events::WindowSelectionEvent{
                                                 window_info.title, window_info.handle});
        Logger().info("Window selected: {}", Utils::String::ToUtf8(window_info.title));
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast window selection data: {}", e.what());
      }
      break;
    }

    case UI::ContextMenu::Types::MenuAction::Type::RatioSelection: {
      try {
        auto ratio_data = std::any_cast<UI::ContextMenu::Types::RatioData>(action.data);
        // 使用新的事件系统发送比例改变事件
        Core::Events::send(*state.events,
                           UI::AppWindow::Events::RatioChangeEvent{
                               ratio_data.index, ratio_data.name, ratio_data.ratio});
        Logger().info("Ratio selected: {} ({})", Utils::String::ToUtf8(ratio_data.name),
                      ratio_data.ratio);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast ratio selection data: {}", e.what());
      }
      break;
    }

    case UI::ContextMenu::Types::MenuAction::Type::ResolutionSelection: {
      try {
        auto resolution_data = std::any_cast<UI::ContextMenu::Types::ResolutionData>(action.data);
        // 使用新的事件系统发送分辨率改变事件
        Core::Events::send(*state.events, UI::AppWindow::Events::ResolutionChangeEvent{
                                                 resolution_data.index, resolution_data.name,
                                                 resolution_data.total_pixels});
        Logger().info("Resolution selected: {} ({}M pixels)",
                      Utils::String::ToUtf8(resolution_data.name),
                      resolution_data.total_pixels / 1000000.0);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast resolution selection data: {}", e.what());
      }
      break;
    }

    case UI::ContextMenu::Types::MenuAction::Type::FeatureToggle: {
      try {
        auto action_id = std::any_cast<std::string>(action.data);

        // 根据action_id确定功能类型和新状态
        if (action_id == "feature.toggle_preview") {
          // 发送预览切换事件
          Core::Events::send(*state.events, UI::AppWindow::Events::PreviewToggleEvent{
                                                   !state.app_window->ui.preview_enabled});
        } else if (action_id == "feature.toggle_overlay") {
          // 发送叠加层切换事件
          Core::Events::send(*state.events, UI::AppWindow::Events::OverlayToggleEvent{
                                                   !state.app_window->ui.overlay_enabled});
        } else if (action_id == "feature.toggle_letterbox") {
          // 发送黑边模式切换事件
          Core::Events::send(*state.events, UI::AppWindow::Events::LetterboxToggleEvent{
                                                   !state.app_window->ui.letterbox_enabled});
        } else if (action_id == "screenshot.capture") {
          // 发送截图事件
          Core::Events::send(*state.events, UI::AppWindow::Events::CaptureEvent{});
        } else if (action_id == "panel.hide") {
          // 发送切换可见性事件
          Core::Events::send(*state.events, UI::AppWindow::Events::ToggleVisibilityEvent{});
        }

        Logger().info("Feature action triggered: {}", action_id);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast feature toggle data: {}", e.what());
      }
      break;
    }

    case UI::ContextMenu::Types::MenuAction::Type::SystemCommand: {
      try {
        auto command = std::any_cast<std::string>(action.data);

        // 根据系统命令发送相应事件
        if (command == "app.exit") {
          // 发送退出事件
          Core::Events::send(*state.events, UI::AppWindow::Events::ExitEvent{});
        } else if (command == "toggle_visibility") {
          // 发送切换可见性事件
          Core::Events::send(*state.events, UI::AppWindow::Events::ToggleVisibilityEvent{});
        } else if (command == "app.webview") {
          // 发送WebView事件
          Core::Events::send(*state.events, UI::AppWindow::Events::WebViewEvent{});
        }

        Logger().info("System command executed: {}", command);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast system command data: {}", e.what());
      }
      break;
    }

    default:
      Logger().warn("Unknown menu action type: {}", static_cast<int>(action.type));
      break;
  }
}

// 隐藏子菜单
auto hide_submenu(Core::State::AppState& state) -> void {
  if (state.context_menu->submenu_hwnd) {
    DestroyWindow(state.context_menu->submenu_hwnd);
    D2DContext::cleanup_submenu(state);
    state.context_menu->submenu_hwnd = nullptr;
    state.context_menu->submenu_parent_index = -1;
    state.context_menu->interaction.submenu_hover_index = -1;
  }
}

// 显示子菜单
auto show_submenu(Core::State::AppState& state, int index) -> void {
  auto& menu_state = *state.context_menu;
  Logger().debug("show_submenu called with index: {}", index);

  // 先隐藏现有的子菜单
  hide_submenu(state);

  // 检查索引是否有效
  if (index < 0 || index >= static_cast<int>(menu_state.items.size())) {
    return;
  }

  const auto& item = menu_state.items[index];
  Logger().debug("Item at index {}: text='{}', has_submenu={}", index,
                 Utils::String::ToUtf8(item.text), item.has_submenu());

  if (!item.has_submenu()) {
    return;
  }

  // 设置父索引，这样get_current_submenu()才能正确返回子菜单项
  menu_state.submenu_parent_index = index;

  // 计算子菜单尺寸和位置
  Layout::calculate_submenu_size(state);
  Layout::calculate_submenu_position(state, index);

  // 创建子菜单窗口
  HINSTANCE instance = state.app_window->window.instance;
  menu_state.submenu_hwnd = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, L"SpinningMomoContextMenuClass",
      L"ContextMenu",  // 窗口标题不重要
      WS_POPUP, menu_state.submenu_position.x, menu_state.submenu_position.y,
      menu_state.submenu_size.cx, menu_state.submenu_size.cy,
      menu_state.hwnd,  // 设置父窗口
      nullptr, instance,
      &state  // 将AppState指针作为创建参数传递
  );

  if (!menu_state.submenu_hwnd) {
    Logger().error("Failed to create submenu window. Error: {}", GetLastError());
    menu_state.submenu_parent_index = -1;  // 重置父索引
    return;
  }

  Logger().debug("Created submenu window: {}", (void*)menu_state.submenu_hwnd);

  // 设置窗口样式
  SetLayeredWindowAttributes(menu_state.submenu_hwnd, 0, 255, LWA_ALPHA);
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(menu_state.submenu_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                        sizeof(corner));

  // 初始化D2D资源
  if (!UI::ContextMenu::D2DContext::initialize_submenu(state, menu_state.submenu_hwnd)) {
    Logger().error("Failed to initialize D2D for submenu.");
    DestroyWindow(menu_state.submenu_hwnd);
    menu_state.submenu_hwnd = nullptr;
    menu_state.submenu_parent_index = -1;  // 重置父索引
    return;
  }

  // 显示窗口
  ShowWindow(menu_state.submenu_hwnd, SW_SHOW);
  SetForegroundWindow(menu_state.submenu_hwnd);
  SetFocus(menu_state.submenu_hwnd);

  Logger().debug("Submenu window shown successfully");
}

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    // 初始化上下文菜单状态
    if (!app_state.context_menu) {
      return std::unexpected("Context menu state is not allocated");
    }

    // 注册窗口类
    if (!register_context_menu_class(app_state.app_window->window.instance,
                                     MessageHandler::static_window_proc)) {
      return std::unexpected("Failed to register context menu window class");
    }

    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception during context menu initialization: ") +
                           e.what());
  }
}

auto cleanup(Core::State::AppState& app_state) -> void {
  // 清理上下文菜单资源
  if (app_state.context_menu) {
    // 销毁任何可能存在的窗口
    if (app_state.context_menu->hwnd) {
      DestroyWindow(app_state.context_menu->hwnd);
      app_state.context_menu->hwnd = nullptr;
    }

    if (app_state.context_menu->submenu_hwnd) {
      DestroyWindow(app_state.context_menu->submenu_hwnd);
      app_state.context_menu->submenu_hwnd = nullptr;
    }

    // 清理D2D资源
    D2DContext::cleanup_context_menu(app_state);
  }
}

auto Show(Core::State::AppState& app_state, std::vector<Types::MenuItem> items,
          const Vendor::Windows::POINT& position) -> void {
  // 1. 更新菜单状态
  auto& menu_state = *app_state.context_menu;
  menu_state.items = std::move(items);
  menu_state.position = position;

  // 检查是否有菜单项
  if (menu_state.items.empty()) {
    Logger().warn("ContextMenu::Show called with no items.");
    return;
  }

  // 2. 创建窗口
  HINSTANCE instance = app_state.app_window->window.instance;
  menu_state.hwnd =
      create_context_menu_window(instance, MessageHandler::static_window_proc, &app_state);

  if (!menu_state.hwnd) {
    Logger().error("Failed to create context menu window.");
    return;
  }

  // 3. 初始化D2D资源
  if (!D2DContext::initialize_context_menu(app_state, menu_state.hwnd)) {
    Logger().error("Failed to initialize D2D for context menu.");
    DestroyWindow(menu_state.hwnd);
    return;
  }

  // 4. 计算布局和最终位置
  Layout::calculate_menu_size(app_state);
  menu_state.position = Layout::calculate_menu_position(app_state, position);

  // 5. 设置窗口最终位置和大小
  SetWindowPos(menu_state.hwnd, HWND_TOPMOST, menu_state.position.x, menu_state.position.y,
               menu_state.menu_size.cx, menu_state.menu_size.cy, SWP_NOACTIVATE);

  // 6. 显示窗口并设置为前景
  ShowWindow(menu_state.hwnd, SW_SHOWNA);
  SetForegroundWindow(menu_state.hwnd);
}

}  // namespace UI::ContextMenu