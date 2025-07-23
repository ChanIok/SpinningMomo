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
import Core.I18n.State;
import Core.I18n.Types;
import Core.Events;
import Common.MenuIds;
import UI.AppWindow.Types;
import UI.AppWindow.State;
import UI.AppWindow.Events;
import UI.TrayMenu.State;
import UI.TrayMenu.Types;
import UI.TrayMenu.Layout;
import UI.TrayMenu.MessageHandler;
import UI.TrayMenu.Painter;
import UI.TrayMenu.D2DContext;
import Utils.Logger;
import Utils.String;
import Vendor.Windows;
import Features.WindowControl;
import Common.MenuData;

namespace {

// 本地化文本访问辅助结构
struct TrayMenuStrings {
  std::wstring select_window;
  std::wstring window_ratio;
  std::wstring resolution;
  std::wstring capture_window;
  std::wstring preview_window;
  std::wstring overlay_window;
  std::wstring open_config;
  std::wstring user_guide;
  std::wstring webview_test;
  std::wstring exit;
};

// 从i18n系统获取tray menu相关的本地化文本
auto get_tray_menu_strings(const Core::I18n::Types::TextData& texts) -> TrayMenuStrings {
  return TrayMenuStrings{.select_window = Utils::String::FromUtf8(texts.menu.window_select),
                         .window_ratio = Utils::String::FromUtf8(texts.menu.window_ratio),
                         .resolution = Utils::String::FromUtf8(texts.menu.window_resolution),
                         .capture_window = Utils::String::FromUtf8(texts.menu.screenshot_capture),
                         .preview_window = Utils::String::FromUtf8(texts.menu.preview_toggle),
                         .overlay_window = Utils::String::FromUtf8(texts.menu.overlay_toggle),
                         .open_config = Utils::String::FromUtf8(texts.menu.settings_config),
                         .user_guide = Utils::String::FromUtf8(texts.menu.app_user_guide),
                         .webview_test = Utils::String::FromUtf8(texts.menu.app_webview_test),
                         .exit = Utils::String::FromUtf8(texts.menu.app_exit)};
}

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
  auto& tray_menu = *state.tray_menu;

  if (tray_menu.is_created) {
    return {};
  }

  // 注册窗口类
  if (!register_tray_menu_class(state.app_window->window.instance)) {
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
                      nullptr, nullptr, state.app_window->window.instance, &state);

  if (!tray_menu.hwnd) {
    return std::unexpected(
        std::format("Failed to create tray menu window, error: {}", GetLastError()));
  }

  // 设置窗口透明度
  SetLayeredWindowAttributes(tray_menu.hwnd, 0, 240, LWA_ALPHA);

  tray_menu.is_created = true;
  return {};
}

// 构建窗口选择子菜单 - 数据驱动版本
auto build_window_submenu(Core::State::AppState& state)
    -> std::vector<UI::TrayMenu::Types::MenuItem> {
  std::vector<UI::TrayMenu::Types::MenuItem> items;

  // 获取所有可见窗口
  auto windows = Features::WindowControl::get_visible_windows();

  for (const auto& window : windows) {
    // 过滤掉系统窗口和不需要的窗口
    if (!window.title.empty() && window.title != L"Program Manager" &&
        window.title.find(L"SpinningMomo") == std::wstring::npos) {
      // 使用工厂方法创建窗口菜单项，直接存储业务数据
      items.emplace_back(UI::TrayMenu::Types::MenuItem::window_item(window));
    }
  }

  // 如果没有找到窗口，添加一个提示项
  if (items.empty()) {
    auto disabled_item = UI::TrayMenu::Types::MenuItem(L"(无可用窗口)");
    disabled_item.is_enabled = false;
    items.emplace_back(std::move(disabled_item));
  }

  return items;
}

// 构建比例选择子菜单 - 数据驱动版本
auto build_ratio_submenu(Core::State::AppState& state)
    -> std::vector<UI::TrayMenu::Types::MenuItem> {
  std::vector<UI::TrayMenu::Types::MenuItem> items;
  const auto& ratios = Common::MenuData::get_current_aspect_ratios(state);

  for (size_t i = 0; i < ratios.size(); ++i) {
    bool is_selected = (i == state.app_window->ui.current_ratio_index);
    items.emplace_back(UI::TrayMenu::Types::MenuItem::ratio_item(ratios[i], i, is_selected));
  }

  return items;
}

// 构建分辨率选择子菜单 - 数据驱动版本
auto build_resolution_submenu(Core::State::AppState& state)
    -> std::vector<UI::TrayMenu::Types::MenuItem> {
  std::vector<UI::TrayMenu::Types::MenuItem> items;
  const auto& resolutions = Common::MenuData::get_current_resolutions(state);

  for (size_t i = 0; i < resolutions.size(); ++i) {
    bool is_selected = (i == state.app_window->ui.current_resolution_index);
    items.emplace_back(
        UI::TrayMenu::Types::MenuItem::resolution_item(resolutions[i], i, is_selected));
  }

  return items;
}

// 初始化菜单项数据 - 重构为完全数据驱动
auto initialize_menu_items(Core::State::AppState& state) -> void {
  auto& tray_menu = *state.tray_menu;
  auto& items = tray_menu.items;

  items.clear();

  const auto strings = get_tray_menu_strings(state.i18n->texts);

  // 窗口选择 - 使用数据驱动的子菜单构建
  auto window_menu = UI::TrayMenu::Types::MenuItem(strings.select_window);
  window_menu.submenu_items = build_window_submenu(state);
  items.emplace_back(std::move(window_menu));

  // 分隔线
  items.emplace_back(UI::TrayMenu::Types::MenuItem::separator());

  // 比例选择 - 使用数据驱动的子菜单构建
  auto ratio_menu = UI::TrayMenu::Types::MenuItem(strings.window_ratio);
  ratio_menu.submenu_items = build_ratio_submenu(state);
  items.emplace_back(std::move(ratio_menu));

  // 分辨率选择 - 使用数据驱动的子菜单构建
  auto resolution_menu = UI::TrayMenu::Types::MenuItem(strings.resolution);
  resolution_menu.submenu_items = build_resolution_submenu(state);
  items.emplace_back(std::move(resolution_menu));

  // 分隔线
  items.emplace_back(UI::TrayMenu::Types::MenuItem::separator());

  // 功能选项 - 使用新的工厂方法
  items.emplace_back(
      UI::TrayMenu::Types::MenuItem::feature_item(strings.capture_window, "screenshot.capture"));
  items.emplace_back(UI::TrayMenu::Types::MenuItem::feature_item(
      strings.preview_window, "feature.toggle_preview", state.app_window->ui.preview_enabled));
  items.emplace_back(UI::TrayMenu::Types::MenuItem::feature_item(
      strings.overlay_window, "feature.toggle_overlay", state.app_window->ui.overlay_enabled));

  // 分隔线
  items.emplace_back(UI::TrayMenu::Types::MenuItem::separator());

  // 系统选项 - 使用新的工厂方法
  items.emplace_back(
      UI::TrayMenu::Types::MenuItem::system_item(strings.open_config, "settings.config"));
  items.emplace_back(
      UI::TrayMenu::Types::MenuItem::system_item(strings.user_guide, "app.user_guide"));
  items.emplace_back(
      UI::TrayMenu::Types::MenuItem::system_item(strings.webview_test, "app.webview_test"));

  // 分隔线
  items.emplace_back(UI::TrayMenu::Types::MenuItem::separator());

  // 退出
  items.emplace_back(UI::TrayMenu::Types::MenuItem::system_item(strings.exit, "app.exit"));
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
  state.tray_menu->layout.update_dpi_scaling(state.app_window->window.dpi);

  // 初始化菜单项
  initialize_menu_items(state);

  // 创建窗口
  if (auto result = create_tray_menu_window(state); !result) {
    return result;
  }

  create_window_attributes(state.tray_menu->hwnd);

  // 创建主菜单D2D资源
  if (!UI::TrayMenu::D2DContext::initialize_main_menu(state, state.tray_menu->hwnd)) {
    return std::unexpected("Failed to initialize D2D for main menu");
  }

  return {};
}

auto cleanup(Core::State::AppState& state) -> void {
  auto& tray_menu = *state.tray_menu;

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
  auto& tray_menu = *state.tray_menu;

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

  // 检查AppWindow的D2D是否已初始化
  if (!state.app_window || !state.app_window->d2d_context.is_initialized) {
    Logger().warn("AppWindow D2D not initialized, tray menu may not render correctly");
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
    if (tray_menu.items[i].type == UI::TrayMenu::Types::MenuItemType::Normal &&
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
  auto& tray_menu = *state.tray_menu;
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
  return state.tray_menu->is_visible;
}

auto handle_menu_command(Core::State::AppState& state, const UI::TrayMenu::Types::MenuItem& item)
    -> void {
  using namespace Core::Events;

  // 隐藏菜单（包括子菜单）
  hide_menu(state);

  // 检查菜单项是否有关联的动作
  if (!item.has_action()) {
    Logger().warn("Menu item '{}' has no associated action", Utils::String::ToUtf8(item.text));
    return;
  }

  const auto& action = item.action.value();

  // 根据动作类型发送相应的事件
  switch (action.type) {
    case UI::TrayMenu::Types::MenuAction::Type::WindowSelection: {
      try {
        auto window_info = std::any_cast<Features::WindowControl::WindowInfo>(action.data);
        // 使用新的事件系统发送窗口选择事件
        Core::Events::send(*state.event_bus, UI::AppWindow::Events::WindowSelectionEvent{
                                                 window_info.title, window_info.handle});
        Logger().info("Window selected: {}", Utils::String::ToUtf8(window_info.title));
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast window selection data: {}", e.what());
      }
      break;
    }

    case UI::TrayMenu::Types::MenuAction::Type::RatioSelection: {
      try {
        auto ratio_data = std::any_cast<UI::TrayMenu::Types::RatioData>(action.data);
        // 使用新的事件系统发送比例改变事件
        Core::Events::send(*state.event_bus,
                           UI::AppWindow::Events::RatioChangeEvent{
                               ratio_data.index, ratio_data.name, ratio_data.ratio});
        Logger().info("Ratio selected: {} ({})", Utils::String::ToUtf8(ratio_data.name),
                      ratio_data.ratio);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast ratio selection data: {}", e.what());
      }
      break;
    }

    case UI::TrayMenu::Types::MenuAction::Type::ResolutionSelection: {
      try {
        auto resolution_data = std::any_cast<UI::TrayMenu::Types::ResolutionData>(action.data);
        // 使用新的事件系统发送分辨率改变事件
        Core::Events::send(*state.event_bus, UI::AppWindow::Events::ResolutionChangeEvent{
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

    case UI::TrayMenu::Types::MenuAction::Type::FeatureToggle: {
      try {
        auto action_id = std::any_cast<std::string>(action.data);

        // 根据action_id确定功能类型和新状态
        if (action_id == "feature.toggle_preview") {
          // 发送预览切换事件
          Core::Events::send(*state.event_bus, UI::AppWindow::Events::PreviewToggleEvent{
                                                   !state.app_window->ui.preview_enabled});
        } else if (action_id == "feature.toggle_overlay") {
          // 发送叠加层切换事件
          Core::Events::send(*state.event_bus, UI::AppWindow::Events::OverlayToggleEvent{
                                                   !state.app_window->ui.overlay_enabled});
        } else if (action_id == "feature.toggle_letterbox") {
          // 发送黑边模式切换事件
          Core::Events::send(*state.event_bus, UI::AppWindow::Events::LetterboxToggleEvent{
                                                   !state.app_window->ui.letterbox_enabled});
        } else if (action_id == "screenshot.capture") {
          // 发送截图事件
          Core::Events::send(*state.event_bus, UI::AppWindow::Events::CaptureEvent{});
        }

        Logger().info("Feature action triggered: {}", action_id);
      } catch (const std::bad_any_cast& e) {
        Logger().error("Failed to cast feature toggle data: {}", e.what());
      }
      break;
    }

    case UI::TrayMenu::Types::MenuAction::Type::SystemCommand: {
      try {
        auto command = std::any_cast<std::string>(action.data);

        // 根据系统命令发送相应事件
        if (command == "app.exit") {
          // 发送退出事件
          Core::Events::send(*state.event_bus, UI::AppWindow::Events::ExitEvent{});
        } else {
          // 使用新的事件系统发送系统命令事件
          Core::Events::send(*state.event_bus, UI::AppWindow::Events::SystemCommandEvent{command});
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

// 计算子菜单尺寸
auto calculate_submenu_size(Core::State::AppState& state) -> void {
  auto& tray_menu = *state.tray_menu;
  const auto& layout = tray_menu.layout;
  const auto& current_submenu = tray_menu.get_current_submenu();

  if (current_submenu.empty()) {
    tray_menu.submenu_size = {0, 0};
    return;
  }

  // 计算子菜单的宽度和高度
  int max_width = layout.min_width;
  int total_height = layout.padding * 2;

  for (const auto& item : current_submenu) {
    if (item.type == UI::TrayMenu::Types::MenuItemType::Separator) {
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
  auto& tray_menu = *state.tray_menu;
  const auto& layout = tray_menu.layout;

  // 计算父菜单项的位置
  int parent_y = layout.padding;
  for (int i = 0; i < parent_index; ++i) {
    const auto& item = tray_menu.items[i];
    if (item.type == UI::TrayMenu::Types::MenuItemType::Separator) {
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
  auto& tray_menu = *state.tray_menu;

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

  // 设置子菜单数据 - 只需设置父项索引，避免数据复制
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
      nullptr, state.app_window->window.instance, &state);

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
  auto& tray_menu = *state.tray_menu;

  if (tray_menu.submenu_hwnd) {
    Logger().debug("Hiding submenu window: {}", (void*)tray_menu.submenu_hwnd);

    // 清理子菜单D2D资源
    UI::TrayMenu::D2DContext::cleanup_submenu(state);

    DestroyWindow(tray_menu.submenu_hwnd);
    tray_menu.submenu_hwnd = nullptr;
    tray_menu.submenu_parent_index = -1;
    tray_menu.interaction.submenu_hover_index = -1;  // 重置子菜单悬停索引

  } else {
    Logger().debug("hide_submenu called but no submenu exists");
  }
}

}  // namespace UI::TrayMenu
