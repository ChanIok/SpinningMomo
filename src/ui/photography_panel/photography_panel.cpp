module;

module UI.PhotographyPanel;

import std;
import Core.State;
import Core.I18n.State;
import UI.PhotographyPanel.MessageHandler;
import UI.PhotographyPanel.Painter;
import UI.PhotographyPanel.RenderContext;
import UI.PhotographyPanel.State;
import Utils.String;
import <dwmapi.h>;
import <windows.h>;

namespace UI::PhotographyPanel {

auto register_window_class(HINSTANCE instance) -> bool {
  static bool registered = false;
  if (registered) {
    return true;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = MessageHandler::static_window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = State::kWindowClassName;
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
  return {work_area.right - window_size.cx - State::kWindowRightMargin,
          work_area.top + State::kWindowTopMargin};
}

// 创建无边框置顶工具窗口，DWM 圆角 + DirectComposition 透明
auto create_window(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto& panel = *state.photography_panel;
  if (panel.hwnd && IsWindow(panel.hwnd)) {
    return {};
  }

  HINSTANCE instance = GetModuleHandleW(nullptr);
  if (!register_window_class(instance)) {
    return std::unexpected("Failed to register photography panel window class");
  }

  panel.layout = UI::PhotographyPanel::Painter::compute_panel_layout(state);
  const POINT position = calculate_panel_position(panel.layout.window_size);
  const auto title = Utils::String::FromUtf8(state.i18n->texts["menu.photography_toggle"]);

  HWND hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              State::kWindowClassName, title.c_str(), WS_POPUP | WS_CLIPCHILDREN,
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
auto show(Core::State::AppState& state) -> std::expected<void, std::string> {
  auto window_result = create_window(state);
  if (!window_result) {
    return window_result;
  }

  auto& panel = *state.photography_panel;
  if (!UI::PhotographyPanel::RenderContext::ensure_render_context(state)) {
    return std::unexpected("Failed to initialize photography panel render context");
  }

  ShowWindow(panel.hwnd, SW_SHOWNA);
  panel.is_visible = true;
  request_repaint(state);
  UpdateWindow(panel.hwnd);
  return {};
}

auto hide(Core::State::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  if (panel.hwnd) {
    ShowWindow(panel.hwnd, SW_HIDE);
  }
  panel.is_visible = false;
  panel.dragging_long_exposure = false;
  panel.knob_hovered = false;
}

auto request_repaint(Core::State::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  if (panel.hwnd && panel.is_visible) {
    InvalidateRect(panel.hwnd, nullptr, FALSE);
  }
}

// 把最新浮窗主题推到摄影面板，保证设置改色后已打开的面板也能立即跟上
auto refresh_from_settings(Core::State::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  panel.layout = UI::PhotographyPanel::Painter::compute_panel_layout(state);

  // 面板的窗口高度、标题高度和 item 节奏都跟随浮窗 layout，一起在这里刷新
  if (panel.hwnd) {
    SetWindowPos(panel.hwnd, nullptr, 0, 0, panel.layout.window_size.cx,
                 panel.layout.window_size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  // 窗口没初始化过时无需补刷，等首次 show 时会按最新设置创建画刷
  if (!panel.render_resources.is_ready) {
    return;
  }

  UI::PhotographyPanel::RenderContext::update_theme_brushes(state);
  UI::PhotographyPanel::RenderContext::update_text_format(state);
  // 只在可见时触发重绘，避免后台窗口因为换肤白白唤醒绘制链路
  if (panel.hwnd && panel.is_visible) {
    request_repaint(state);
  }
}

// 销毁窗口（触发 WM_NCDESTROY 清理 D3D），若窗口已不存在则直接释放渲染资源
auto cleanup(Core::State::AppState& state) -> void {
  auto& panel = *state.photography_panel;
  if (panel.hwnd) {
    DestroyWindow(panel.hwnd);
    panel.hwnd = nullptr;
  } else {
    UI::PhotographyPanel::RenderContext::cleanup_render_context(state);
  }
  panel.is_visible = false;
  panel.dragging_long_exposure = false;
  panel.knob_hovered = false;
}

}  // namespace UI::PhotographyPanel
