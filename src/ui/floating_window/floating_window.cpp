#include "ui/floating_window/floating_window.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/dwmapi.hpp"
#include "vendor/windows/windowsx.hpp"

#include "core/commands/registry.hpp"
#include "core/commands/types.hpp"
#include "core/events/events.hpp"
#include "core/i18n/state.hpp"
#include "core/i18n/types.hpp"
#include "core/state/app_state.hpp"
#include "features/settings/menu.hpp"
#include "features/settings/state.hpp"
#include "features/settings/types.hpp"
#include "ui/floating_window/layout.hpp"
#include "ui/floating_window/message_handler.hpp"
#include "ui/floating_window/painter.hpp"
#include "ui/floating_window/render_context.hpp"
#include "ui/floating_window/state.hpp"
#include "ui/floating_window/types.hpp"
#include "utils/logger/logger.hpp"
#include "utils/string/string.hpp"

namespace ui::floating_window {

// 用于前台窗口变化回调，用于 Windows 11 TopMost Z 序失效 workaround
static core::AppState* g_floating_window_for_topmost_hook = nullptr;

static void CALLBACK topmost_refresh_win_event_proc(HWINEVENTHOOK /*hook*/, DWORD event, HWND hwnd,
                                                    LONG /*idObject*/, LONG /*idChild*/,
                                                    DWORD /*idEventThread*/,
                                                    DWORD /*dwmsEventTime*/) {
  if (event != EVENT_SYSTEM_FOREGROUND || !g_floating_window_for_topmost_hook) {
    return;
  }
  auto& state = *g_floating_window_for_topmost_hook;
  if (!state.floating_window->window.is_visible || !state.floating_window->window.hwnd) {
    return;
  }
  // 当前台变为本窗口时，无需刷新
  if (hwnd == state.floating_window->window.hwnd) {
    return;
  }
  // 当前台为本进程的其他窗口（如上下文菜单、WebView）时，不刷新，避免覆盖自己的 UI
  DWORD fg_pid = 0;
  GetWindowThreadProcessId(hwnd, &fg_pid);
  if (fg_pid != 0 && fg_pid == GetCurrentProcessId()) {
    return;
  }
  // 当前台变为外部应用时，请求刷新置顶状态以恢复 Z 序
  PostMessageW(state.floating_window->window.hwnd, ui::floating_window::WM_REFRESH_TOPMOST, 0, 0);
}

auto create_window(core::AppState& state) -> std::expected<void, std::string> {
  // 获取系统DPI
  UINT dpi = 96;
  if (HDC hdc = GetDC(nullptr); hdc) {
    dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
  }

  // 保存DPI到状态中
  state.floating_window->window.dpi = dpi;
  const auto metrics = ui::floating_window::layout::calculate_window_metrics(state, dpi);
  state.floating_window->layout = metrics.layout;

  // 初始化菜单项
  initialize_menu_items(state);

  // 计算窗口尺寸和位置
  const auto window_size = metrics.size;
  const auto window_pos = ui::floating_window::layout::calculate_center_position(window_size);

  register_window_class(state.floating_window->window.instance);

  // 由 DirectComposition 托管像素内容时，窗口本身不再走 DWM 的 redirection bitmap。
  state.floating_window->window.hwnd = CreateWindowExW(
      WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      L"SpinningMomoFloatingWindowClass", L"SpinningMomo", WS_POPUP | WS_CLIPCHILDREN, window_pos.x,
      window_pos.y, window_size.cx, window_size.cy, nullptr, nullptr,
      state.floating_window->window.instance, &state);

  if (!state.floating_window->window.hwnd) {
    return std::unexpected("Failed to create window");
  }

  // 允许低权限进程发送显示窗口的消息（绕过 UIPI 限制）
  ChangeWindowMessageFilterEx(state.floating_window->window.hwnd, 0x8000 + 100, MSGFLT_ALLOW,
                              nullptr);

  // 保存窗口尺寸和位置
  state.floating_window->window.size = window_size;
  state.floating_window->window.position = window_pos;

  // 创建窗口属性
  create_window_attributes(state.floating_window->window.hwnd);
  refresh_visible_frame_border_thickness(state);

  // 初始化Direct2D渲染
  if (!ui::floating_window::render_context::initialize_render_context(
          state, state.floating_window->window.hwnd)) {
    Logger().error("Failed to initialize Direct2D rendering");
  }

  return {};
}

// 标准化渲染触发机制
auto request_repaint(core::AppState& state) -> void {
  if (state.floating_window->window.hwnd && state.floating_window->window.is_visible) {
    InvalidateRect(state.floating_window->window.hwnd, nullptr, FALSE);
  }
}

auto refresh_visible_frame_border_thickness(core::AppState& state) -> void {
  auto& window = state.floating_window->window;
  UINT thickness = 0;
  if (window.hwnd) {
    DwmGetWindowAttribute(window.hwnd, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS, &thickness,
                          sizeof(thickness));
  }
  window.visible_frame_border_thickness = thickness;
}

auto install_topmost_refresh_hook(core::AppState& state) -> void {
  auto& win = state.floating_window->window;
  if (win.topmost_refresh_hook) {
    return;  // 已安装
  }
  g_floating_window_for_topmost_hook = &state;
  win.topmost_refresh_hook =
      SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                      topmost_refresh_win_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT);
  if (!win.topmost_refresh_hook) {
    Logger().warn("Failed to install topmost refresh hook for Windows 11 workaround");
    g_floating_window_for_topmost_hook = nullptr;
  }
}

auto uninstall_topmost_refresh_hook(core::AppState& state) -> void {
  auto& win = state.floating_window->window;
  if (win.topmost_refresh_hook) {
    UnhookWinEvent(win.topmost_refresh_hook);
    win.topmost_refresh_hook = nullptr;
  }
  if (g_floating_window_for_topmost_hook == &state) {
    g_floating_window_for_topmost_hook = nullptr;
  }
}

auto show_window(core::AppState& state) -> void {
  if (state.floating_window->window.hwnd) {
    ShowWindow(state.floating_window->window.hwnd, SW_SHOWNA);
    state.floating_window->window.is_visible = true;
    refresh_visible_frame_border_thickness(state);

    // Windows 11 TopMost Z 序失效 workaround：显示时安装前台变化监听
    install_topmost_refresh_hook(state);

    // 先标记脏区再同步 UpdateWindow，确保启动后续初始化继续执行前完成首绘。
    request_repaint(state);
    UpdateWindow(state.floating_window->window.hwnd);
  }
}

auto hide_window(core::AppState& state) -> void {
  if (state.floating_window->window.hwnd) {
    uninstall_topmost_refresh_hook(state);
    ShowWindow(state.floating_window->window.hwnd, SW_HIDE);
    state.floating_window->window.is_visible = false;
  }
}

auto toggle_visibility(core::AppState& state) -> void {
  if (state.floating_window->window.is_visible) {
    hide_window(state);
  } else {
    show_window(state);
  }
}

auto destroy_window(core::AppState& state) -> void {
  // 清理Direct2D资源
  ui::floating_window::render_context::cleanup_render_context(state);

  if (state.floating_window->window.hwnd) {
    uninstall_topmost_refresh_hook(state);
    DestroyWindow(state.floating_window->window.hwnd);
    state.floating_window->window.hwnd = nullptr;
    state.floating_window->window.is_visible = false;
  }
}

auto set_current_ratio(core::AppState& state, size_t index) -> void {
  state.floating_window->ui.current_ratio_index = index;
  if (state.floating_window->window.hwnd) {
    request_repaint(state);
  }
}

auto set_current_resolution(core::AppState& state, size_t index) -> void {
  const auto& resolutions = features::settings::menu::get_resolutions(state);
  if (index < resolutions.size()) {
    state.floating_window->ui.current_resolution_index = index;
    if (state.floating_window->window.hwnd) {
      request_repaint(state);
    }
  }
}

auto update_menu_items(core::AppState& state) -> void {
  state.floating_window->data.menu_items.clear();
  initialize_menu_items(state);
  if (state.floating_window->window.hwnd) {
    request_repaint(state);
  }
}

// 内部辅助函数实现

// 根据 i18n_key 获取本地化文本（扁平化版本）
auto get_text_by_i18n_key(const std::string& i18n_key, const core::i18n::TextData& texts)
    -> std::wstring {
  auto it = texts.find(i18n_key);
  if (it != texts.end()) {
    return utils::string::FromUtf8(it->second);
  }
  // Fallback: 返回 key 本身
  return utils::string::FromUtf8(i18n_key);
}

auto normalize_scroll_offsets(core::AppState& state) -> void {
  const size_t page_size = static_cast<size_t>(state.floating_window->layout.max_visible_rows);
  if (page_size == 0) {
    state.floating_window->ui.ratio_scroll_offset = 0;
    state.floating_window->ui.resolution_scroll_offset = 0;
    state.floating_window->ui.feature_scroll_offset = 0;
    return;
  }

  size_t ratio_count = 0;
  size_t resolution_count = 0;
  size_t feature_count = 0;
  for (const auto& item : state.floating_window->data.menu_items) {
    switch (item.category) {
      case ui::floating_window::MenuItemCategory::AspectRatio:
        ++ratio_count;
        break;
      case ui::floating_window::MenuItemCategory::Resolution:
        ++resolution_count;
        break;
      case ui::floating_window::MenuItemCategory::Feature:
        ++feature_count;
        break;
    }
  }

  const auto clamp_offset = [page_size](size_t& offset, size_t item_count) -> void {
    if (item_count == 0) {
      offset = 0;
      return;
    }
    const size_t max_page = (item_count - 1) / page_size;
    const size_t current_page = offset / page_size;
    offset = std::min(current_page, max_page) * page_size;
  };

  auto& ui = state.floating_window->ui;
  clamp_offset(ui.ratio_scroll_offset, ratio_count);
  clamp_offset(ui.resolution_scroll_offset, resolution_count);
  clamp_offset(ui.feature_scroll_offset, feature_count);
}

auto register_window_class(HINSTANCE instance) -> void {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = message_handler::static_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = L"SpinningMomoFloatingWindowClass";
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassExW(&wc);
}

auto initialize_menu_items(core::AppState& state) -> void {
  state.floating_window->data.menu_items.clear();

  // 获取比例和分辨率预设
  const auto& ratios = features::settings::menu::get_ratios(state);
  const auto& resolutions = features::settings::menu::get_resolutions(state);

  // 从配置获取功能项顺序
  const auto& feature_config = state.settings->raw.ui.app_menu.features;
  const auto& texts = state.i18n->texts;

  // 添加比例选项
  for (size_t i = 0; i < ratios.size(); ++i) {
    state.floating_window->data.menu_items.emplace_back(
        ratios[i].name, ui::floating_window::MenuItemCategory::AspectRatio, static_cast<int>(i));
  }

  // 添加分辨率选项
  for (size_t i = 0; i < resolutions.size(); ++i) {
    const auto& preset = resolutions[i];
    state.floating_window->data.menu_items.emplace_back(
        preset.name, ui::floating_window::MenuItemCategory::Resolution, static_cast<int>(i));
  }

  // 添加功能项（从命令注册表获取）
  for (size_t i = 0; i < feature_config.size(); ++i) {
    const auto& command_id = feature_config[i];
    // 从注册表获取命令描述
    if (const auto* command = core::commands::get_command(state, command_id)) {
      // 使用 i18n_key 获取文本
      std::wstring text = get_text_by_i18n_key(command->i18n_key, texts);
      state.floating_window->data.menu_items.emplace_back(
          text, ui::floating_window::MenuItemCategory::Feature, static_cast<int>(i), command_id);
    } else {
      Logger().warn("Command not found in registry: {}", command_id);
    }
  }
}

auto create_window_attributes(HWND hwnd) -> void {
  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

// 设置变更响应实现
auto refresh_from_settings(core::AppState& state) -> void {
  // 更新菜单项
  update_menu_items(state);

  const auto metrics = ui::floating_window::layout::calculate_window_metrics(
      state, state.floating_window->window.dpi);
  state.floating_window->layout = metrics.layout;

  // 行数配置变化后，确保翻页偏移仍落在有效页
  normalize_scroll_offsets(state);

  // 更新颜色配置
  ui::floating_window::render_context::update_all_brush_colors(state);

  const auto new_size = metrics.size;
  if (state.floating_window->window.hwnd) {
    // 调整窗口大小
    SetWindowPos(state.floating_window->window.hwnd, nullptr, 0, 0, new_size.cx, new_size.cy,
                 SWP_NOMOVE | SWP_NOZORDER);
    state.floating_window->window.size = new_size;
    // 日志输出大小
    Logger().info("Window size updated: {}x{}", new_size.cx, new_size.cy);
  }

  state.floating_window->render_resources.needs_font_update = true;

  // 请求重绘
  request_repaint(state);
}

}  // namespace ui::floating_window
