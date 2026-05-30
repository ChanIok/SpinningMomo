module;

module Features.Notifications.Draw;

import std;
import Core.State;
import Features.Notifications.State;
import Features.Notifications.Constants;
import Features.Notifications.Types;
import Features.Settings.State;
import UI.FloatingWindow.State;
import Utils.Logger;
import Utils.System;
import <d2d1_3.h>;
import <dwrite_3.h>;
import <windows.h>;

namespace Features::Notifications {

// ============================================================
// 视觉样式（仅影响绘制，不参与状态机）
// ============================================================

// 视觉样式只影响绘制，不参与布局/状态机语义。
// 单宿主窗口里的每个 toast 都是 D2D 图形，不是真正的 HWND；
// 因此 Windows 11 的圆角、描边、阴影需要在绘制层模拟。
struct NotificationVisualStyle {
  bool use_system_chrome = false;
  float corner_radius = 0.0f;
  float border_width = 0.0f;
  int shadow_margin = 0;
  D2D1_COLOR_F border_color{};
};

// ============================================================
// DPI 辅助
// ============================================================

// Windows 把 96 DPI 定为"100% 缩放"基准。所有设计尺寸均以 96 DPI 为单位编写，
// 在高 DPI 屏幕上通过 MulDiv 等比放大，保证物理尺寸一致。
auto scale_for_dpi(int value, int dpi) -> int { return MulDiv(value, dpi, 96); }

// 返回 1 个物理像素对应的 D2D 逻辑宽度（D2D 坐标系以物理像素为单位）。
// 用于绘制 1px 精细描边，在高 DPI 屏幕上同样保持单像素锐利。
auto stroke_width_for_dpi(int dpi) -> float { return static_cast<float>(scale_for_dpi(1, dpi)); }

// ============================================================
// 通用谓词 / 几何辅助
// ============================================================

// FadingOut 和 Done 统称"正在退出"——这两个状态下通知不参与布局计算。
auto is_exiting(Types::NotificationAnimState state) -> bool {
  return state == Types::NotificationAnimState::FadingOut ||
         state == Types::NotificationAnimState::Done;
}

// GDI RECT（整数坐标）转 D2D 浮点矩形。D2D 坐标系以亚像素精度工作，故需显式转换。
auto rect_to_d2d(const RECT& rect) -> D2D1_RECT_F {
  return D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top),
                     static_cast<float>(rect.right), static_cast<float>(rect.bottom));
}

// 在已有颜色的 alpha 通道上乘以 opacity，用于让整张卡片随动画渐隐渐显。
// 乘法而非替换，是为了保留颜色本身的半透明度（如背景色自带 alpha）。
auto color_with_opacity(D2D1_COLOR_F color, float opacity) -> D2D1_COLOR_F {
  color.a *= std::clamp(opacity, 0.0f, 1.0f);
  return color;
}
auto is_windows_11_or_newer() -> bool {
  // 系统版本在进程生命周期内不会变化，缓存结果避免每帧绘制都查询 ntdll。
  static const bool result = []() -> bool {
    const auto version = Utils::System::get_windows_version();
    if (!version) {
      return false;
    }
    return version->major_version >= 10 && version->build_number >= 22000;
  }();
  return result;
}

auto resolve_visual_style(int dpi) -> NotificationVisualStyle {
  if (!is_windows_11_or_newer()) {
    // Windows 10 的浮窗风格是直角、无描边、无阴影；
    // 通知也跟随这个规则，避免跨系统视觉不一致。
    return {};
  }

  const float border_gray = static_cast<float>(Constants::BASE_BORDER_GRAY) / 255.0f;
  return NotificationVisualStyle{
      .use_system_chrome = true,
      .corner_radius = static_cast<float>(scale_for_dpi(Constants::BASE_CORNER_RADIUS, dpi)),
      .border_width = stroke_width_for_dpi(dpi),
      .shadow_margin = scale_for_dpi(10, dpi),
      .border_color =
          D2D1::ColorF(border_gray, border_gray, border_gray, Constants::BASE_BORDER_ALPHA),
  };
}

// ============================================================
// 颜色解析（支持用户自定义主题色）
// ============================================================

// 将单个十六进制字符转为 0~15 的整数，无效字符返回 -1。
auto hex_char_to_int(char c) -> int {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// 将 "#RRGGBB" 或 "#RRGGBBAA" 格式的字符串解析为 D2D 颜色。
// 解析失败（格式错误、字符串为空）时返回 fallback，保证通知始终有合法颜色。
auto parse_hex_color(std::string_view hex_color, D2D1_COLOR_F fallback) -> D2D1_COLOR_F {
  if (hex_color.empty()) return fallback;
  if (hex_color.starts_with('#')) {
    hex_color.remove_prefix(1);
  }
  if (hex_color.size() < 6) return fallback;

  const int r_hi = hex_char_to_int(hex_color[0]);
  const int r_lo = hex_char_to_int(hex_color[1]);
  const int g_hi = hex_char_to_int(hex_color[2]);
  const int g_lo = hex_char_to_int(hex_color[3]);
  const int b_hi = hex_char_to_int(hex_color[4]);
  const int b_lo = hex_char_to_int(hex_color[5]);
  if (r_hi < 0 || r_lo < 0 || g_hi < 0 || g_lo < 0 || b_hi < 0 || b_lo < 0) {
    return fallback;
  }

  // 项目设置使用 #RRGGBBAA；没有 AA 时按完全不透明处理。
  float alpha = fallback.a;
  if (hex_color.size() >= 8) {
    const int a_hi = hex_char_to_int(hex_color[6]);
    const int a_lo = hex_char_to_int(hex_color[7]);
    if (a_hi >= 0 && a_lo >= 0) {
      alpha = static_cast<float>((a_hi << 4) | a_lo) / 255.0f;
    }
  } else {
    alpha = 1.0f;
  }

  return D2D1::ColorF(static_cast<float>((r_hi << 4) | r_lo) / 255.0f,
                      static_cast<float>((g_hi << 4) | g_lo) / 255.0f,
                      static_cast<float>((b_hi << 4) | b_lo) / 255.0f, alpha);
}

auto resolve_notification_theme_colors(const Core::State::AppState& state)
    -> Types::NotificationThemeColors {
  // 通知创建时会复制一份颜色快照，避免 toast 生命周期内设置变化导致外观跳变。
  Types::NotificationThemeColors colors{
      .background = D2D1::ColorF(0.12f, 0.12f, 0.12f, 0.82f),
      .text = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f),
      .hover = D2D1::ColorF(0.31f, 0.31f, 0.31f, 0.80f),
  };

  const auto& settings_colors = state.settings->raw.ui.floating_window_colors;
  colors.background = parse_hex_color(settings_colors.background, colors.background);
  colors.text = parse_hex_color(settings_colors.text, colors.text);
  colors.hover = parse_hex_color(settings_colors.hover, colors.hover);
  return colors;
}

// ============================================================
// DPI / 布局尺寸辅助
// ============================================================

auto get_current_dpi(const Core::State::AppState& state) -> int {
  // 通知宿主窗口创建前，优先借用浮窗保存的 DPI；
  // 宿主窗口创建后，直接以宿主窗口 DPI 为准。
  if (state.notifications->host_hwnd) {
    return static_cast<int>(GetDpiForWindow(state.notifications->host_hwnd));
  }
  if (state.floating_window->window.dpi > 0) {
    return static_cast<int>(state.floating_window->window.dpi);
  }
  if (HDC hdc = GetDC(nullptr); hdc) {
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return dpi > 0 ? dpi : 96;
  }
  return 96;
}

// 通知卡片的宽度，与浮窗宽度保持一致，随 DPI 缩放。
auto get_window_width(int dpi) -> int { return scale_for_dpi(Constants::BASE_WINDOW_WIDTH, dpi); }

auto get_layout_margin(int dpi) -> int {
  // Windows 11 下需要给模拟阴影留出额外透明边距，否则阴影会被宿主窗口裁掉。
  const auto style = resolve_visual_style(dpi);
  return scale_for_dpi(Constants::BASE_PADDING, dpi) + style.shadow_margin;
}

auto get_host_size(int dpi) -> SIZE {
  // 宿主窗口不是一张 toast，而是右下角整组 toast 的透明画布。
  // 这里按最大可见数量预留空间，避免连续通知时频繁改变窗口大小。
  const int margin = get_layout_margin(dpi);
  const int spacing = scale_for_dpi(Constants::BASE_SPACING, dpi);
  const int width = get_window_width(dpi) + margin * 2;
  const int height =
      scale_for_dpi(Constants::BASE_MAX_HEIGHT, dpi) * Constants::MAX_VISIBLE_NOTIFICATIONS +
      spacing * (Constants::MAX_VISIBLE_NOTIFICATIONS - 1) + margin * 2;
  return {width, height};
}

// ============================================================
// 渲染资源管理
// ============================================================
//
// D2D 渲染管线由以下层次构成：
//   ID2D1Factory7           ← 创建渲染目标的工厂（进程级单例）
//   IDWriteFactory7         ← 创建文本格式/布局的工厂（进程级共享）
//   HDC (memory_dc)         ← GDI 离屏 DC，作为 D2D 的"画板"
//   HBITMAP (dib_bitmap)    ← 32 位带 alpha 的 DIB，实际像素存储在这里
//   ID2D1DCRenderTarget     ← D2D 渲染目标，绑定到 memory_dc
//   IDWriteTextFormat*      ← 标题、正文、按钮各一份文本格式
//
// 所有 COM 对象均手动管理生命周期（Release），没有使用智能指针，
// 因为它们随 NotificationRenderSurface 一起存活，生命周期清晰。

// 创建一个 DirectWrite 文本格式对象。字体固定用微软雅黑，以保证中文字符正常渲染。
// 三个样式参数分别控制水平对齐、垂直对齐、换行方式。
auto create_text_format(IDWriteFactory7* write_factory, float font_size,
                        DWRITE_TEXT_ALIGNMENT text_alignment,
                        DWRITE_PARAGRAPH_ALIGNMENT paragraph_alignment,
                        DWRITE_WORD_WRAPPING word_wrapping) -> IDWriteTextFormat* {
  if (!write_factory) {
    return nullptr;
  }

  IDWriteTextFormat* format = nullptr;
  const HRESULT hr = write_factory->CreateTextFormat(
      L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, font_size, L"zh-CN", &format);
  if (FAILED(hr) || !format) {
    return nullptr;
  }

  format->SetTextAlignment(text_alignment);
  format->SetParagraphAlignment(paragraph_alignment);
  format->SetWordWrapping(word_wrapping);
  return format;
}

// 释放三个文本格式对象（COM 对象需手动 Release，置 nullptr 防止悬空指针）。
auto release_text_formats(State::NotificationRenderSurface& surface) -> void {
  if (surface.title_text_format) {
    surface.title_text_format->Release();
    surface.title_text_format = nullptr;
  }
  if (surface.message_text_format) {
    surface.message_text_format->Release();
    surface.message_text_format = nullptr;
  }
  if (surface.button_text_format) {
    surface.button_text_format->Release();
    surface.button_text_format = nullptr;
  }
}

// 释放 DIB 位图。GDI 要求：必须先用 SelectObject 将旧 bitmap 恢复回 DC，
// 才能安全 DeleteObject，否则 bitmap 处于"被选中"状态，删除会失败。
auto cleanup_surface_bitmap(State::NotificationRenderSurface& surface) -> void {
  if (surface.old_bitmap && surface.memory_dc) {
    SelectObject(surface.memory_dc, surface.old_bitmap);
    surface.old_bitmap = nullptr;
  }
  if (surface.dib_bitmap) {
    DeleteObject(surface.dib_bitmap);
    surface.dib_bitmap = nullptr;
  }
  surface.bitmap_bits = nullptr;
  surface.bitmap_size = {};
}

auto release_notification_render_surface(State::NotificationRenderSurface& surface) -> void {
  // D2D DC render target 绑定的是一张 GDI DIB；
  // 清理时要先解绑 SelectObject 选入的旧 bitmap，再释放 DIB 和 DC。
  release_text_formats(surface);

  if (surface.render_target) {
    surface.render_target->Release();
    surface.render_target = nullptr;
  }
  if (surface.device_context) {
    surface.device_context->Release();
    surface.device_context = nullptr;
  }

  cleanup_surface_bitmap(surface);

  if (surface.memory_dc) {
    DeleteDC(surface.memory_dc);
    surface.memory_dc = nullptr;
  }
  if (surface.write_factory) {
    surface.write_factory->Release();
    surface.write_factory = nullptr;
  }
  if (surface.factory) {
    surface.factory->Release();
    surface.factory = nullptr;
  }

  surface.is_ready = false;
  surface.is_rendering = false;
}

auto ensure_factories(State::NotificationRenderSurface& surface) -> bool {
  // 工厂和文本工厂是整个通知宿主窗口共用的昂贵资源，按需创建并复用。
  if (!surface.factory) {
    const HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7),
                                         nullptr, reinterpret_cast<void**>(&surface.factory));
    if (FAILED(hr)) {
      return false;
    }
  }

  if (!surface.write_factory) {
    const HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
                                           reinterpret_cast<IUnknown**>(&surface.write_factory));
    if (FAILED(hr)) {
      return false;
    }
  }

  return true;
}

// 创建一个与屏幕兼容的离屏 DC（内存设备上下文）。
// CreateCompatibleDC 需要一个参考 DC 来继承颜色格式，用完后立即释放屏幕 DC。
auto ensure_memory_dc(State::NotificationRenderSurface& surface) -> bool {
  if (surface.memory_dc) {
    return true;
  }

  HDC screen_dc = GetDC(nullptr);
  surface.memory_dc = CreateCompatibleDC(screen_dc);
  if (screen_dc) {
    ReleaseDC(nullptr, screen_dc);
  }
  return surface.memory_dc != nullptr;
}

// 创建绑定到 GDI DC 的 D2D 渲染目标（DC Render Target）。
// 使用预乘 alpha（PREMULTIPLIED）格式是 UpdateLayeredWindow 的要求。
// 同时尝试获取 ID2D1DeviceContext6 接口以使用高级混合模式，失败时降级使用基础功能。
auto ensure_render_target(State::NotificationRenderSurface& surface) -> bool {
  if (surface.render_target) {
    return true;
  }

  D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0,
      D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);

  if (FAILED(surface.factory->CreateDCRenderTarget(&props, &surface.render_target))) {
    return false;
  }

  surface.render_target->QueryInterface(__uuidof(ID2D1DeviceContext6),
                                        reinterpret_cast<void**>(&surface.device_context));

  return true;
}

// 确保 DIB（设备无关位图）与宿主窗口的当前尺寸匹配。
// 尺寸不变时直接复用；尺寸变化时重新分配，并将 D2D 渲染目标重新绑定到新 DIB。
// biHeight 设为负数是 Win32 惯例，表示像素从上到下存储（top-down DIB）。
auto ensure_bitmap(State::NotificationRenderSurface& surface, const SIZE& size) -> bool {
  if (size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  if (surface.bitmap_size.cx == size.cx && surface.bitmap_size.cy == size.cy &&
      surface.dib_bitmap) {
    return true;
  }

  if (!ensure_memory_dc(surface)) {
    return false;
  }

  cleanup_surface_bitmap(surface);

  // 分层窗口需要带 alpha 的 32 位 DIB；D2D 先画到这里，再一次性提交给 DWM。
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = size.cx;
  bmi.bmiHeader.biHeight = -size.cy;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  surface.dib_bitmap =
      CreateDIBSection(surface.memory_dc, &bmi, DIB_RGB_COLORS, &surface.bitmap_bits, nullptr, 0);
  if (!surface.dib_bitmap) {
    return false;
  }

  surface.old_bitmap = SelectObject(surface.memory_dc, surface.dib_bitmap);
  surface.bitmap_size = size;

  RECT binding_rect{0, 0, size.cx, size.cy};
  return surface.render_target &&
         SUCCEEDED(surface.render_target->BindDC(surface.memory_dc, &binding_rect));
}

auto ensure_text_formats(State::NotificationRenderSurface& surface, int dpi) -> bool {
  // 字体大小跟随 DPI；DPI 改变时重建文本格式即可，不需要重建整个通知状态。
  if (surface.title_text_format && surface.message_text_format && surface.button_text_format &&
      surface.dpi == dpi) {
    return true;
  }

  release_text_formats(surface);

  surface.title_text_format = create_text_format(
      surface.write_factory,
      static_cast<float>(scale_for_dpi(Constants::BASE_TITLE_FONT_SIZE, dpi)),
      DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
  surface.message_text_format = create_text_format(
      surface.write_factory, static_cast<float>(scale_for_dpi(Constants::BASE_FONT_SIZE, dpi)),
      DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
      DWRITE_WORD_WRAPPING_CHARACTER);
  surface.button_text_format = create_text_format(
      surface.write_factory, static_cast<float>(scale_for_dpi(Constants::BASE_FONT_SIZE, dpi)),
      DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
      DWRITE_WORD_WRAPPING_NO_WRAP);

  surface.dpi = dpi;
  return surface.title_text_format && surface.message_text_format && surface.button_text_format;
}

auto ensure_render_surface(Core::State::AppState& state) -> bool {
  // 这是绘制前的统一入口：保证工厂、render target、DIB 和字体都处于可用状态。
  // 任何一步失败都清理整套 surface，下次绘制再从干净状态重建。
  auto& surface = state.notifications->surface;
  if (!state.notifications->host_hwnd) {
    return false;
  }

  RECT client_rect{};
  GetClientRect(state.notifications->host_hwnd, &client_rect);
  const SIZE size{client_rect.right - client_rect.left, client_rect.bottom - client_rect.top};
  const int dpi = get_current_dpi(state);

  if (!ensure_factories(surface) || !ensure_render_target(surface) ||
      !ensure_bitmap(surface, size) || !ensure_text_formats(surface, dpi)) {
    release_notification_render_surface(surface);
    return false;
  }

  surface.is_ready = true;
  return true;
}

// ============================================================
// 文本测量
// ============================================================

// 测量 text 在给定宽度 width 内自动换行后的实际高度。
// DirectWrite 需要先创建一个临时 TextLayout 对象才能查询排版后的尺寸，用完立即释放。
auto measure_text_height(State::NotificationRenderSurface& surface, const std::wstring& text,
                         float width) -> float {
  if (!surface.write_factory || !surface.message_text_format || width <= 0.0f) {
    return 0.0f;
  }

  IDWriteTextLayout* layout = nullptr;
  const HRESULT hr = surface.write_factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.length()), surface.message_text_format, width,
      10000.0f, &layout);
  if (FAILED(hr) || !layout) {
    return 0.0f;
  }

  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  layout->Release();
  return metrics.height;
}

// 测量单行文本 text 在不限宽度时的自然宽度，用于计算按钮的最小合适宽度。
auto measure_text_width(State::NotificationRenderSurface& surface, const std::wstring& text,
                        IDWriteTextFormat* format) -> float {
  if (!surface.write_factory || !format || text.empty()) {
    return 0.0f;
  }

  IDWriteTextLayout* layout = nullptr;
  const HRESULT hr = surface.write_factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.length()), format, 10000.0f, 10000.0f, &layout);
  if (FAILED(hr) || !layout) {
    return 0.0f;
  }

  DWRITE_TEXT_METRICS metrics{};
  layout->GetMetrics(&metrics);
  layout->Release();
  return metrics.width;
}

// 按钮宽度 = max(最小固定宽度, 文本自然宽度 + 左右内边距)，同时不超过 max_width。
// 若渲染表面尚未就绪（首次显示前），退回最小宽度。
auto calculate_button_width(Core::State::AppState& state, const std::wstring& label, int max_width)
    -> int {
  const int dpi = get_current_dpi(state);
  const int min_width = scale_for_dpi(Constants::BASE_BUTTON_WIDTH, dpi);
  const int text_padding = scale_for_dpi(Constants::BASE_BUTTON_TEXT_PADDING, dpi);
  int width = min_width;

  if (state.notifications->surface.is_ready && state.notifications->surface.button_text_format) {
    width = std::max(min_width, static_cast<int>(std::ceil(measure_text_width(
                                    state.notifications->surface, label,
                                    state.notifications->surface.button_text_format))) +
                                    text_padding * 2);
  }

  return std::min(width, std::max(min_width, max_width));
}

// 测量正文区高度。若渲染表面还未就绪（例如首次 show_notification 时），
// 退回单行字体高度作为保守估算，避免通知高度为 0。
auto measure_message_block(Core::State::AppState& state, const std::wstring& message,
                           float message_text_width) -> float {
  const int font_size = scale_for_dpi(Constants::BASE_FONT_SIZE, get_current_dpi(state));
  float message_height = static_cast<float>(font_size);
  if (message_text_width <= 0.0f) {
    return message_height;
  }
  if (ensure_render_surface(state)) {
    message_height = std::max(message_height, measure_text_height(state.notifications->surface,
                                                                  message, message_text_width));
  }
  return message_height;
}

// ============================================================
// 布局计算
// ============================================================

// 计算一张通知卡片的布局尺寸。
// 卡片内部采用两列布局：左列放标题+正文，右列放可选的 action 按钮。
// 如果有按钮，正文可用宽度会相应减少；卡片高度取左列和右列的较高者。
auto compute_notification_layout(Core::State::AppState& state, const std::wstring& message,
                                 const std::optional<Types::NotificationAction>& action,
                                 int card_width) -> Types::NotificationLayoutMetrics {
  const int dpi = get_current_dpi(state);
  Types::NotificationLayoutMetrics metrics{
      .padding = scale_for_dpi(Constants::BASE_PADDING, dpi),
      .content_padding = scale_for_dpi(Constants::BASE_CONTENT_PADDING, dpi),
      .column_gap = scale_for_dpi(Constants::BASE_ACTION_COLUMN_GAP, dpi),
      .title_height = scale_for_dpi(Constants::BASE_TITLE_HEIGHT, dpi),
      .title_message_gap = scale_for_dpi(Constants::BASE_TITLE_MESSAGE_GAP, dpi),
      .button_height = scale_for_dpi(Constants::BASE_BUTTON_HEIGHT, dpi),
  };

  if (action) {
    const int max_width = card_width - metrics.content_padding * 2 - metrics.column_gap;
    metrics.action_width = calculate_button_width(state, action->label, max_width);
  }

  const float message_text_width =
      static_cast<float>(card_width - metrics.content_padding * 2 -
                         (action ? metrics.action_width + metrics.column_gap : 0));
  const float message_height = measure_message_block(state, message, message_text_width);
  const int left_height = metrics.title_height + metrics.title_message_gap +
                          static_cast<int>(std::ceil(message_height));
  metrics.content_height = std::max(left_height, action ? metrics.button_height : 0);
  return metrics;
}

// 根据布局内容高度计算卡片最终高度，限制在 [BASE_MIN_HEIGHT, BASE_MAX_HEIGHT] 范围内，
// 避免消息过短时卡片过小或消息过长时撑破布局。
auto measure_card_height(const Types::NotificationLayoutMetrics& layout, int dpi) -> int {
  const int total_height = layout.padding * 2 + layout.content_height;
  const int min_height = scale_for_dpi(Constants::BASE_MIN_HEIGHT, dpi);
  const int max_height = scale_for_dpi(Constants::BASE_MAX_HEIGHT, dpi);
  return std::clamp(total_height, min_height, max_height);
}

// 过滤掉 label 为空的 action，避免绘制一个没有文字的按钮。
auto normalize_action(std::optional<Types::NotificationAction> action)
    -> std::optional<Types::NotificationAction> {
  if (!action || action->label.empty()) {
    return std::nullopt;
  }
  return action;
}

// ============================================================
// D2D 绘制原语
// ============================================================
//
// D2D 绘制所有形状都需要先创建一个 Brush（画刷），用完后必须 Release。
// 下面的辅助函数统一封装"创建画刷 → 绘制 → 释放"这个固定模式。

// 创建单色实心画刷，若失败（设备丢失等）返回 nullptr，调用方据此跳过绘制。
auto create_solid_brush(ID2D1DCRenderTarget* target, D2D1_COLOR_F color) -> ID2D1SolidColorBrush* {
  ID2D1SolidColorBrush* brush = nullptr;
  if (!target || FAILED(target->CreateSolidColorBrush(color, &brush))) {
    return nullptr;
  }
  return brush;
}

auto fill_rounded_rect(ID2D1DCRenderTarget* target, const D2D1_ROUNDED_RECT& rect,
                       D2D1_COLOR_F color) -> void {
  if (auto* brush = create_solid_brush(target, color)) {
    target->FillRoundedRectangle(rect, brush);
    brush->Release();
  }
}

auto draw_rounded_rect(ID2D1DCRenderTarget* target, const D2D1_ROUNDED_RECT& rect,
                       D2D1_COLOR_F color, float stroke_width) -> void {
  if (auto* brush = create_solid_brush(target, color)) {
    target->DrawRoundedRectangle(rect, brush, stroke_width);
    brush->Release();
  }
}

// 向内收缩圆角矩形（四边各收 inset），并同步缩小圆角半径。
// D2D 描边是以路径为中心绘制的，收缩一半描边宽度可使描边完全落在原矩形内侧。
auto inset_rounded_rect(const D2D1_ROUNDED_RECT& rect, float inset) -> D2D1_ROUNDED_RECT {
  const auto& bounds = rect.rect;
  const float radius = std::max(0.0f, rect.radiusX - inset);
  return D2D1::RoundedRect(D2D1::RectF(bounds.left + inset, bounds.top + inset,
                                       bounds.right - inset, bounds.bottom - inset),
                           radius, radius);
}

auto draw_stroked_rounded_rect(ID2D1DCRenderTarget* target, const D2D1_ROUNDED_RECT& rect,
                               D2D1_COLOR_F color, float stroke_width) -> void {
  if (stroke_width <= 0.0f) {
    return;
  }
  draw_rounded_rect(target, inset_rounded_rect(rect, stroke_width / 2.0f), color, stroke_width);
}

auto expanded_rounded_rect(const D2D1_RECT_F& rect, float spread, float y_offset, float radius)
    -> D2D1_ROUNDED_RECT {
  // 底部延伸保持 spread + y_offset；上/左/右收窄，避免四周均匀晕开。
  constexpr float side_spread_scale = 0.2f;
  constexpr float top_spread_scale = 0.1f;
  const float side_spread = spread * side_spread_scale;
  const float top_spread = spread * top_spread_scale;
  const auto expanded = D2D1::RectF(rect.left - side_spread, rect.top - top_spread + y_offset,
                                    rect.right + side_spread, rect.bottom + spread + y_offset);
  return D2D1::RoundedRect(expanded, radius + spread, radius + spread);
}

auto draw_card_shadow(ID2D1DCRenderTarget* target, const D2D1_RECT_F& rect,
                      const NotificationVisualStyle& style, float opacity) -> void {
  if (!style.use_system_chrome || style.shadow_margin <= 0 || opacity <= 0.0f) {
    return;
  }

  struct ShadowLayer {
    float spread;
    float y_offset;
    float alpha;
  };

  // D2D DC render target 没有系统投影 API，这里用几层低透明度圆角矩形模拟轻阴影。
  constexpr std::array<ShadowLayer, 4> layers{{
      {.spread = 1.0f, .y_offset = 1.0f, .alpha = 0.06f},
      {.spread = 3.0f, .y_offset = 2.0f, .alpha = 0.04f},
      {.spread = 6.0f, .y_offset = 4.0f, .alpha = 0.025f},
      {.spread = 10.0f, .y_offset = 7.0f, .alpha = 0.014f},
  }};

  for (const auto& layer : layers) {
    fill_rounded_rect(
        target, expanded_rounded_rect(rect, layer.spread, layer.y_offset, style.corner_radius),
        D2D1::ColorF(0.0f, 0.0f, 0.0f, layer.alpha * opacity));
  }
}

auto draw_text(ID2D1DCRenderTarget* target, std::wstring_view text, IDWriteTextFormat* format,
               const D2D1_RECT_F& rect, D2D1_COLOR_F color) -> void {
  if (!target || !format || text.empty()) {
    return;
  }

  if (auto* brush = create_solid_brush(target, color)) {
    target->DrawText(text.data(), static_cast<UINT32>(text.length()), format, rect, brush,
                     D2D1_DRAW_TEXT_OPTIONS_CLIP);
    brush->Release();
  }
}

// ============================================================
// 矩形同步（由状态变更方负责调用，绘制前必须保证已更新）
// ============================================================

// 根据当前 current_pos 和缓存的 layout 重新计算所有子矩形（card / content / title / message /
// action）。 这些矩形既用于绘制，也用于鼠标命中测试，位置改变后必须立即同步。
auto update_notification_rects(State::Notification& notification) -> void {
  // 左右布局：左列为标题+正文，右列为单个 action；矩形只依赖缓存 layout 与 current_pos。
  const auto& layout = notification.layout;
  const bool has_action = notification.action.has_value();

  notification.card_rect = {notification.current_pos.x, notification.current_pos.y,
                            notification.current_pos.x + notification.width,
                            notification.current_pos.y + notification.height};

  const int inner_top = notification.card_rect.top + layout.padding;
  const int inner_bottom = notification.card_rect.top + layout.padding + layout.content_height;
  const int content_right = has_action ? notification.card_rect.right - layout.content_padding -
                                             layout.action_width - layout.column_gap
                                       : notification.card_rect.right - layout.content_padding;

  notification.content_rect = {notification.card_rect.left + layout.content_padding, inner_top,
                               content_right, inner_bottom};
  notification.title_rect = {notification.content_rect.left, notification.content_rect.top,
                             notification.content_rect.right,
                             notification.content_rect.top + layout.title_height};
  notification.message_rect = {
      notification.content_rect.left,
      notification.content_rect.top + layout.title_height + layout.title_message_gap,
      notification.content_rect.right, notification.content_rect.bottom};

  if (has_action) {
    const int action_left =
        notification.card_rect.right - layout.content_padding - layout.action_width;
    const int action_top = inner_top + (layout.content_height - layout.button_height) / 2;
    notification.action_rect = {action_left, action_top, action_left + layout.action_width,
                                action_top + layout.button_height};
  } else {
    notification.action_rect = {};
  }
}

auto update_all_notification_rects(Core::State::AppState& state) -> void {
  for (auto& notification : state.notifications->active_notifications) {
    update_notification_rects(notification);
  }
}

// ============================================================
// 卡片绘制
// ============================================================

// 绘制 action 按钮区域：未悬停时半透明背景，悬停时加深 + 加描边以示可交互。
auto draw_action_button(Core::State::AppState& state, const State::Notification& notification)
    -> void {
  if (!notification.action) {
    return;
  }

  auto& surface = state.notifications->surface;
  const int dpi = get_current_dpi(state);
  const auto style = resolve_visual_style(dpi);
  const float hover_radius = style.corner_radius / 2.0f;
  const auto rect = rect_to_d2d(notification.action_rect);
  const auto rounded = D2D1::RoundedRect(rect, hover_radius, hover_radius);
  D2D1_COLOR_F button_bg = notification.colors.hover;
  if (notification.action_hovered) {
    button_bg.a = 1.0f;
  }
  fill_rounded_rect(surface.render_target, rounded,
                    color_with_opacity(button_bg, notification.opacity));
  if (notification.action_hovered) {
    draw_stroked_rounded_rect(surface.render_target, rounded,
                              color_with_opacity(notification.colors.text, notification.opacity),
                              stroke_width_for_dpi(dpi));
  }

  draw_text(surface.render_target, notification.action->label, surface.button_text_format, rect,
            color_with_opacity(notification.colors.text, notification.opacity));
}

auto draw_notification(Core::State::AppState& state, const State::Notification& notification)
    -> void {
  // 绘制顺序决定视觉层级：阴影 -> 卡片背景 -> 描边 -> 左列文本 -> 右列按钮。
  auto& surface = state.notifications->surface;
  if (notification.opacity <= 0.0f || notification.state == Types::NotificationAnimState::Done) {
    return;
  }

  const int dpi = get_current_dpi(state);
  const auto style = resolve_visual_style(dpi);
  const D2D1_RECT_F card_rect = rect_to_d2d(notification.card_rect);

  const auto card_rounded = D2D1::RoundedRect(card_rect, style.corner_radius, style.corner_radius);
  draw_card_shadow(surface.render_target, card_rect, style, notification.opacity);
  fill_rounded_rect(surface.render_target, card_rounded,
                    color_with_opacity(notification.colors.background, notification.opacity));
  if (style.border_width > 0.0f) {
    draw_stroked_rounded_rect(surface.render_target, card_rounded,
                              color_with_opacity(style.border_color, notification.opacity),
                              style.border_width);
  }

  draw_text(surface.render_target, notification.title, surface.title_text_format,
            rect_to_d2d(notification.title_rect),
            color_with_opacity(notification.colors.text, notification.opacity));
  draw_text(surface.render_target, notification.message, surface.message_text_format,
            rect_to_d2d(notification.message_rect),
            color_with_opacity(notification.colors.text, notification.opacity));
  draw_action_button(state, notification);
}

auto present_render_surface(Core::State::AppState& state) -> void {
  // 提交整张透明画布。ULW_ALPHA 会使用 DIB 的每像素 alpha，
  // 所以卡片外部、阴影外部都可以保持完全透明。
  auto& surface = state.notifications->surface;
  if (!state.notifications->host_hwnd || !surface.memory_dc || surface.bitmap_size.cx <= 0 ||
      surface.bitmap_size.cy <= 0) {
    return;
  }

  BLENDFUNCTION blend{};
  blend.BlendOp = AC_SRC_OVER;
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;

  POINT src_point{0, 0};
  SIZE size = surface.bitmap_size;
  UpdateLayeredWindow(state.notifications->host_hwnd, nullptr, nullptr, &size, surface.memory_dc,
                      &src_point, 0, &blend, ULW_ALPHA);
}

auto paint_notifications(Core::State::AppState& state) -> void {
  // 每一帧都重绘整组 toast，而不是为每条通知维护独立窗口。
  // 这样移动动画、层级和透明穿透都在同一套坐标系内完成。
  auto& surface = state.notifications->surface;
  if (state.notifications->active_notifications.empty() || !ensure_render_surface(state) ||
      surface.is_rendering) {
    return;
  }

  surface.is_rendering = true;
  surface.render_target->BeginDraw();
  surface.render_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  // 与浮窗一致：COPY 混合避免半透明图层相互叠加导致颜色偏差。
  if (surface.device_context) {
    surface.device_context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
  }

  for (const auto& notification : state.notifications->active_notifications) {
    draw_notification(state, notification);
  }

  const HRESULT hr = surface.render_target->EndDraw();
  surface.is_rendering = false;

  if (hr == D2DERR_RECREATE_TARGET) {
    release_notification_render_surface(surface);
    Logger().warn("Notification render target needs recreation");
    return;
  }
  if (FAILED(hr)) {
    Logger().error("Notification paint error: 0x{:X}", hr);
    return;
  }

  present_render_surface(state);
}

// 仅当宿主窗口当前可见时才触发重绘（鼠标事件等非 timer 路径调用）。
// 窗口不可见时不绘制，避免浪费 D2D 资源。
auto request_repaint(Core::State::AppState& state) -> void {
  if (state.notifications->host_hwnd && IsWindowVisible(state.notifications->host_hwnd)) {
    paint_notifications(state);
  }
}

auto cleanup_render_surface(Core::State::AppState& state) -> void {
  release_notification_render_surface(state.notifications->surface);
}

}  // namespace Features::Notifications
