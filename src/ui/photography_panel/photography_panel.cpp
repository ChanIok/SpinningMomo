#include "ui/photography_panel/photography_panel.hpp"

#include "vendor/std.hpp"

#include "vendor/windows.hpp"
#include "vendor/windows/dwmapi.hpp"

#include "core/i18n/state.hpp"
#include "core/state/app_state.hpp"
#include "ui/photography_panel/message_handler.hpp"
#include "ui/photography_panel/painter.hpp"
#include "ui/photography_panel/render_context.hpp"
#include "ui/photography_panel/state.hpp"
#include "utils/string/string.hpp"

namespace ui::photography_panel {

auto register_window_class(HINSTANCE instance) -> bool {
  static bool registered = false;
  if (registered) {
    return true;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = message_handler::static_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.style = CS_HREDRAW | CS_VREDRAW;

  if (!RegisterClassExW(&wc)) {
    if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return false;
    }
  }

  registered = true;
  return true;
}

// 面板定位在屏幕右上角，距离边缘 24px，顶部留 48px 避开系统 UI
auto calculate_panel_position(const SIZE& window_size) -> POINT {
  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  return {work_area.right - window_size.cx - kWindowRightMargin, work_area.top + kWindowTopMargin};
}

// 创建无边框置顶工具窗口，DWM 圆角 + DirectComposition 透明
auto create_window(core::AppState& state) -> std::expected<void, std::string> {
  auto& panel = *state.photography_panel;
  if (panel.hwnd && IsWindow(panel.hwnd)) {
    return {};
  }

  HINSTANCE instance = GetModuleHandleW(nullptr);
  if (!register_window_class(instance)) {
    return std::unexpected("Failed to register photography panel window class");
  }

  panel.layout = ui::photography_panel::painter::compute_panel_layout(state);
  const POINT position = calculate_panel_position(panel.layout.window_size);
  const auto title = utils::string::FromUtf8(state.i18n->texts["menu.photography_toggle"]);

  HWND hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              kWindowClassName, title.c_str(), WS_POPUP | WS_CLIPCHILDREN,
                              position.x, position.y, panel.layout.window_size.cx,
                              panel.layout.window_size.cy, nullptr, nullptr, instance, &state);
  if (!hwnd) {
    return std::unexpected("Failed to create photography panel window");
  }

  DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

  panel.hwnd = hwnd;
  return {};
}

// 创建窗口 + 初始化 D3D 渲染上下文 + 显示
auto show(core::AppState& state) -> std::expected<void, std::string> {
  auto window_result = create_window(state);
  if (!window_result) {
    return window_result;
  }

  auto& panel = *state.photography_panel;
  if (!ui::photography_panel::render_context::ensure_render_context(state)) {
    return std::unexpected("Failed to initialize photography panel render context");
  }

  ShowWindow(panel.hwnd, SW_SHOWNA);
  panel.is_visible = true;
  request_repaint(state);
  UpdateWindow(panel.hwnd);
  return {};
}

auto hide(core::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  if (panel.hwnd) {
    ShowWindow(panel.hwnd, SW_HIDE);
  }
  panel.is_visible = false;
  panel.dragging_long_exposure = false;
  panel.knob_hovered = false;
}

auto request_repaint(core::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  if (panel.hwnd && panel.is_visible) {
    InvalidateRect(panel.hwnd, nullptr, FALSE);
  }
}

// 把最新浮窗主题推到摄影面板，保证设置改色后已打开的面板也能立即跟上
auto refresh_from_settings(core::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  panel.layout = ui::photography_panel::painter::compute_panel_layout(state);

  // 面板的窗口高度、标题高度和 item 节奏都跟随浮窗 layout，一起在这里刷新
  if (panel.hwnd) {
    SetWindowPos(panel.hwnd, nullptr, 0, 0, panel.layout.window_size.cx,
                 panel.layout.window_size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  // 窗口没初始化过时无需补刷，等首次 show 时会按最新设置创建画刷
  if (!panel.render_resources.is_ready) {
    return;
  }

  ui::photography_panel::render_context::update_theme_brushes(state);
  ui::photography_panel::render_context::update_text_format(state);
  // 只在可见时触发重绘，避免后台窗口因为换肤白白唤醒绘制链路
  if (panel.hwnd && panel.is_visible) {
    request_repaint(state);
  }
}

// 销毁窗口（触发 WM_NCDESTROY 清理 D3D），若窗口已不存在则直接释放渲染资源
auto cleanup(core::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  if (panel.hwnd) {
    DestroyWindow(panel.hwnd);
    panel.hwnd = nullptr;
  } else {
    ui::photography_panel::render_context::cleanup_render_context(state);
  }
  panel.is_visible = false;
  panel.dragging_long_exposure = false;
  panel.knob_hovered = false;
}

}  // namespace ui::photography_panel
