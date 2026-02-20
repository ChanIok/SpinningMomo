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
import Features.Settings.Menu;
import Core.Commands;
import Core.Commands.State;
import UI.FloatingWindow.Types;
import UI.FloatingWindow.State;
import UI.FloatingWindow.Events;
import UI.ContextMenu.State;
import UI.ContextMenu.Types;
import UI.ContextMenu.Layout;
import UI.ContextMenu.MessageHandler;
import UI.ContextMenu.Interaction;
import UI.ContextMenu.Painter;
import UI.ContextMenu.D2DContext;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;
import Features.WindowControl;

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

  // 重置交互状态，避免旧菜单残留的hover/定时意图影响下一次显示。
  UI::ContextMenu::Interaction::reset(state);
  state.context_menu->submenu_parent_index = -1;
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
        Core::Events::send(*state.events, UI::FloatingWindow::Events::WindowSelectionEvent{
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
        Core::Events::send(*state.events, UI::FloatingWindow::Events::RatioChangeEvent{
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
        Core::Events::send(*state.events, UI::FloatingWindow::Events::ResolutionChangeEvent{
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

    case UI::ContextMenu::Types::MenuAction::Type::FeatureToggle:
    case UI::ContextMenu::Types::MenuAction::Type::SystemCommand: {
      try {
        auto action_id = std::any_cast<std::string>(action.data);

        // 通过注册表调用命令
        if (state.commands) {
          Core::Commands::invoke_command(state.commands->registry, action_id);
        }

        Logger().info("Feature action triggered: {}", action_id);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast action data: {}", e.what());
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
  HINSTANCE instance = state.floating_window->window.instance;
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
    if (!register_context_menu_class(app_state.floating_window->window.instance,
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
    UI::ContextMenu::Interaction::reset(app_state);
    app_state.context_menu->submenu_parent_index = -1;
  }
}

auto Show(Core::State::AppState& app_state, std::vector<Types::MenuItem> items,
          const Vendor::Windows::POINT& position) -> void {
  // 若已有菜单实例，先回收，确保状态机从干净状态重新开始。
  if (app_state.context_menu->hwnd || app_state.context_menu->submenu_hwnd) {
    hide_and_destroy_menu(app_state);
  }

  // 1. 更新菜单状态
  auto& menu_state = *app_state.context_menu;
  UI::ContextMenu::Interaction::reset(app_state);
  menu_state.submenu_parent_index = -1;
  menu_state.items = std::move(items);
  menu_state.position = position;

  // 检查是否有菜单项
  if (menu_state.items.empty()) {
    Logger().warn("ContextMenu::Show called with no items.");
    return;
  }

  // 2. 创建窗口
  HINSTANCE instance = app_state.floating_window->window.instance;
  menu_state.hwnd =
      create_context_menu_window(instance, MessageHandler::static_window_proc, &app_state);

  if (!menu_state.hwnd) {
    Logger().error("Failed to create context menu window.");
    return;
  }

  // 3. 应用 DPI 缩放（必须在 D2D 初始化之前，因为 text_format 创建依赖 layout.font_size）
  UINT dpi = app_state.floating_window->window.dpi;
  menu_state.layout.update_dpi_scaling(dpi);

  // 4. 初始化D2D资源
  if (!D2DContext::initialize_context_menu(app_state, menu_state.hwnd)) {
    Logger().error("Failed to initialize D2D for context menu.");
    DestroyWindow(menu_state.hwnd);
    return;
  }

  // 5. 计算布局和最终位置
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
