#include "ui/context_menu/context_menu.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/d2d1.hpp"
#include "vendor/windows/dwmapi.hpp"
#include "vendor/windows/dwrite.hpp"
#include "vendor/windows/wrl/client.hpp"

#include "core/commands/registry.hpp"
#include "core/commands/types.hpp"
#include "core/events/events.hpp"
#include "core/i18n/state.hpp"
#include "core/i18n/types.hpp"
#include "core/state/app_state.hpp"
#include "features/settings/menu.hpp"
#include "features/window_control/window_control.hpp"
#include "ui/context_menu/interaction.hpp"
#include "ui/context_menu/layout.hpp"
#include "ui/context_menu/message_handler.hpp"
#include "ui/context_menu/painter.hpp"
#include "ui/context_menu/render_context.hpp"
#include "ui/context_menu/state.hpp"
#include "ui/context_menu/types.hpp"
#include "ui/floating_window/events.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/floating_window/types.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace ui::context_menu {

auto apply_corner_preference(HWND hwnd) -> void {
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

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

auto create_context_menu_window(HINSTANCE instance, core::AppState* app_state, HWND owner,
                                const POINT& position, const SIZE& size) -> HWND {
  HWND hwnd = CreateWindowExW(
      WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"SpinningMomoContextMenuClass",
      L"ContextMenu",  // 窗口标题不重要
      WS_POPUP, position.x, position.y, size.cx, size.cy, owner, nullptr, instance,
      app_state  // 将AppState指针作为创建参数传递
  );

  if (hwnd) {
    apply_corner_preference(hwnd);
  }

  return hwnd;
}

// 隐藏并销毁菜单窗口
void hide_and_destroy_menu(core::AppState& state) {
  // 先销毁子菜单
  if (state.context_menu->submenu_hwnd) {
    DestroyWindow(state.context_menu->submenu_hwnd);
    state.context_menu->submenu_hwnd = nullptr;
    // 确保清理子菜单D2D资源
    render_context::cleanup_submenu(state);
  }

  // 再销毁主菜单
  if (state.context_menu->hwnd) {
    DestroyWindow(state.context_menu->hwnd);
    state.context_menu->hwnd = nullptr;
    // 确保清理主菜单D2D资源
    render_context::cleanup_context_menu(state);
  }

  // 重置交互状态，避免旧菜单残留的hover/定时意图影响下一次显示。
  ui::context_menu::interaction::reset(state);
  state.context_menu->submenu_parent_index = -1;
}

// 处理菜单命令
void handle_menu_action(core::AppState& state, const ui::context_menu::MenuItem& item) {
  if (!item.has_action()) {
    Logger().warn("Menu item '{}' has no associated action", utils::string::ToUtf8(item.text));
    return;
  }
  const auto& action = item.action.value();

  // 根据动作类型发送相应的事件
  switch (action.type) {
    case ui::context_menu::MenuAction::Type::WindowSelection: {
      try {
        auto window_info = std::any_cast<features::window_control::WindowInfo>(action.data);
        // 使用新的事件系统发送窗口选择事件
        core::events::send(state, ui::floating_window::events::WindowSelectionEvent{
                                      window_info.title, window_info.handle});
        Logger().info("Window selected: {}", utils::string::ToUtf8(window_info.title));
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast window selection data: {}", e.what());
      }
      break;
    }

    case ui::context_menu::MenuAction::Type::RatioSelection: {
      try {
        auto ratio_data = std::any_cast<ui::context_menu::RatioData>(action.data);
        // 使用新的事件系统发送比例改变事件
        core::events::send(state, ui::floating_window::events::RatioChangeEvent{
                                      ratio_data.index, ratio_data.name, ratio_data.ratio});
        Logger().info("Ratio selected: {} ({})", utils::string::ToUtf8(ratio_data.name),
                      ratio_data.ratio);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast ratio selection data: {}", e.what());
      }
      break;
    }

    case ui::context_menu::MenuAction::Type::ResolutionSelection: {
      try {
        auto resolution_data = std::any_cast<ui::context_menu::ResolutionData>(action.data);
        // 使用新的事件系统发送分辨率改变事件
        core::events::send(state, ui::floating_window::events::ResolutionChangeEvent{
                                      resolution_data.index, resolution_data.name});
        Logger().info("Resolution selected: {}", utils::string::ToUtf8(resolution_data.name));
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast resolution selection data: {}", e.what());
      }
      break;
    }

    case ui::context_menu::MenuAction::Type::FeatureToggle:
    case ui::context_menu::MenuAction::Type::SystemCommand: {
      // 两者都携带 action_id 字符串，走 Commands 注册表统一派发
      try {
        auto action_id = std::any_cast<std::string>(action.data);

        core::commands::invoke_command(state, action_id);

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
auto hide_submenu(core::AppState& state) -> void {
  if (state.context_menu->submenu_hwnd) {
    DestroyWindow(state.context_menu->submenu_hwnd);
    render_context::cleanup_submenu(state);
    state.context_menu->submenu_hwnd = nullptr;
    state.context_menu->submenu_parent_index = -1;
    state.context_menu->interaction.submenu_hover_index = -1;
  }
}

// 显示指定项的子菜单：复用已有窗口或创建新窗口 → 布局与计算尺寸 → 绘制内容 → 提交淡入/展示
auto show_submenu(core::AppState& state, int index) -> void {
  auto& menu_state = *state.context_menu;
  Logger().debug("show_submenu called with index: {}", index);

  // 检查索引是否有效
  if (index < 0 || index >= static_cast<int>(menu_state.items.size())) {
    return;
  }

  const auto& item = menu_state.items[index];
  Logger().debug("Item at index {}: text='{}', has_submenu={}", index,
                 utils::string::ToUtf8(item.text), item.has_submenu());

  if (!item.has_submenu()) {
    return;
  }

  if (menu_state.submenu_hwnd && menu_state.submenu_parent_index == index) {
    return;
  }

  const int previous_parent_index = menu_state.submenu_parent_index;
  const SIZE previous_size = menu_state.submenu_size;
  const POINT previous_position = menu_state.submenu_position;

  // 设置父索引，这样get_current_submenu()才能正确返回子菜单项
  menu_state.submenu_parent_index = index;

  // 计算子菜单尺寸和位置
  layout::calculate_submenu_size(state);
  layout::calculate_submenu_position(state, index);

  if (menu_state.submenu_hwnd) {
    // 同级切换保留窗口和合成资源，避免销毁旧窗口后露出桌面
    // SetWindowPos 会同步触发 WM_SIZE，因此必须先准备好完整的新菜单状态
    const int previous_hover_index = menu_state.interaction.submenu_hover_index;
    menu_state.interaction.submenu_hover_index = -1;

    RECT previous_rect{};
    GetClientRect(menu_state.submenu_hwnd, &previous_rect);
    const bool size_changed =
        previous_rect.right - previous_rect.left != menu_state.submenu_size.cx ||
        previous_rect.bottom - previous_rect.top != menu_state.submenu_size.cy;

    // 调整现有子菜单窗口位置与尺寸
    if (!SetWindowPos(menu_state.submenu_hwnd, nullptr, menu_state.submenu_position.x,
                      menu_state.submenu_position.y, menu_state.submenu_size.cx,
                      menu_state.submenu_size.cy, SWP_NOACTIVATE | SWP_NOZORDER)) {
      Logger().error("Failed to reposition submenu window. Error: {}", GetLastError());
      menu_state.submenu_parent_index = previous_parent_index;
      menu_state.submenu_size = previous_size;
      menu_state.submenu_position = previous_position;
      menu_state.interaction.submenu_hover_index = previous_hover_index;
      return;
    }
    // 尺寸变化由 WM_SIZE 调整交换链并立即绘制；同尺寸切换也需要主动提交新内容
    if (!size_changed) {
      RECT client_rect{0, 0, menu_state.submenu_size.cx, menu_state.submenu_size.cy};
      painter::paint_submenu(state, client_rect);
    }
    // 同级切换直接设为不透明显示，避免重复淡入晃眼
    if (!render_context::show_surface(menu_state.submenu_render_resources, false)) {
      hide_submenu(state);
    }
    return;
  }

  menu_state.interaction.submenu_hover_index = -1;

  // 创建子菜单窗口
  HINSTANCE instance = state.floating_window->window.instance;
  menu_state.submenu_hwnd = create_context_menu_window(
      instance, &state, menu_state.hwnd, menu_state.submenu_position, menu_state.submenu_size);

  if (!menu_state.submenu_hwnd) {
    Logger().error("Failed to create submenu window. Error: {}", GetLastError());
    menu_state.submenu_parent_index = -1;  // 重置父索引
    return;
  }

  Logger().debug("Created submenu window: {}", (void*)menu_state.submenu_hwnd);

  // 初始化D2D资源
  if (!ui::context_menu::render_context::initialize_submenu(state, menu_state.submenu_hwnd)) {
    Logger().error("Failed to initialize D2D for submenu.");
    DestroyWindow(menu_state.submenu_hwnd);
    menu_state.submenu_hwnd = nullptr;
    menu_state.submenu_parent_index = -1;  // 重置父索引
    return;
  }

  // 先绘制子菜单首帧内容；初始化时 visual 保持透明以防闪白
  RECT client_rect{0, 0, menu_state.submenu_size.cx, menu_state.submenu_size.cy};
  ui::context_menu::painter::paint_submenu(state, client_rect);

  // 触发 DComp 硬件淡入动画
  if (!render_context::show_surface(menu_state.submenu_render_resources, true)) {
    hide_submenu(state);
    return;
  }

  // 显示窗口
  ShowWindow(menu_state.submenu_hwnd, SW_SHOW);
  SetForegroundWindow(menu_state.submenu_hwnd);
  SetFocus(menu_state.submenu_hwnd);

  Logger().debug("Submenu window shown successfully");
}

// 注册菜单窗口类，应用启动时调用一次
auto initialize(core::AppState& app_state) -> std::expected<void, std::string> {
  try {
    // 初始化上下文菜单状态
    if (!app_state.context_menu) {
      return std::unexpected("Context menu state is not allocated");
    }

    // 注册窗口类
    if (!register_context_menu_class(app_state.floating_window->window.instance,
                                     message_handler::static_window_proc)) {
      return std::unexpected("Failed to register context menu window class");
    }

    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Exception during context menu initialization: ") +
                           e.what());
  }
}

// 释放菜单子系统全部资源，应用退出时调用
auto cleanup(core::AppState& app_state) -> void {
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
    render_context::cleanup_submenu(app_state);
    render_context::cleanup_context_menu(app_state);
    ui::context_menu::interaction::reset(app_state);
    app_state.context_menu->submenu_parent_index = -1;
  }
}

// 主入口：回收旧实例 → 布局计算 → 创建窗口 → D2D 初始化 → 淡入 → 显示
auto Show(core::AppState& app_state, std::vector<MenuItem> items, const POINT& position) -> void {
  // 若已有菜单实例，先回收，确保状态机从干净状态重新开始。
  if (app_state.context_menu->hwnd || app_state.context_menu->submenu_hwnd) {
    hide_and_destroy_menu(app_state);
  }

  // 1. 更新菜单状态
  auto& menu_state = *app_state.context_menu;
  ui::context_menu::interaction::reset(app_state);
  menu_state.submenu_parent_index = -1;
  menu_state.items = std::move(items);
  menu_state.position = position;

  // 检查是否有菜单项
  if (menu_state.items.empty()) {
    Logger().warn("ContextMenu::Show called with no items.");
    return;
  }

  // 2. 创建窗口
  // 2. 应用 DPI 缩放
  UINT dpi = app_state.floating_window->window.dpi;
  menu_state.layout.update_dpi_scaling(dpi);

  if (!render_context::initialize_text_format(app_state)) {
    Logger().error("Failed to initialize text format for context menu.");
    return;
  }

  // 3. 计算布局和最终位置
  layout::calculate_menu_size(app_state);
  menu_state.position = layout::calculate_menu_position(app_state, position);

  // 4. 创建窗口（直接使用最终位置和尺寸）
  HINSTANCE instance = app_state.floating_window->window.instance;
  menu_state.hwnd = create_context_menu_window(instance, &app_state, nullptr, menu_state.position,
                                               menu_state.menu_size);

  if (!menu_state.hwnd) {
    Logger().error("Failed to create context menu window.");
    return;
  }

  // 5. 初始化D2D资源
  if (!render_context::initialize_context_menu(app_state, menu_state.hwnd)) {
    Logger().error("Failed to initialize D2D for context menu.");
    DestroyWindow(menu_state.hwnd);
    menu_state.hwnd = nullptr;
    return;
  }

  // 绘制主菜单首帧内容；初始化时 visual 保持透明以防闪白
  RECT client_rect{0, 0, menu_state.menu_size.cx, menu_state.menu_size.cy};
  painter::paint_context_menu(app_state, client_rect);

  // 触发 DComp 硬件淡入动画
  if (!render_context::show_surface(menu_state.main_render_resources, true)) {
    hide_and_destroy_menu(app_state);
    return;
  }

  // 6. 显示窗口并设置为前景
  ShowWindow(menu_state.hwnd, SW_SHOWNA);
  SetForegroundWindow(menu_state.hwnd);
}

}  // namespace ui::context_menu
