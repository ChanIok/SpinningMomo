module;

module UI.FloatingWindow.D2DContext;

import Core.State;
import UI.FloatingWindow.State;
import UI.FloatingWindow.Types;
import Features.Settings.State;
import <d2d1_3.h>;
import <d3d11.h>;
import <dcomp.h>;
import <dwrite_3.h>;
import <dxgi1_2.h>;
import <wil/com.h>;
import <windows.h>;

namespace UI::FloatingWindow::D2DContext {

constexpr const char* kRecordingIndicatorColor = "#ED4C4CFF";
constexpr DXGI_FORMAT kSurfaceFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

// 浮窗渲染后端已切到：
//   D3D11 device -> composition swap chain -> DirectComposition visual tree
//   -> D2D device context 直接绘制到 DXGI surface
// 这样可以彻底绕开旧的 DCRenderTarget + HDC + UpdateLayeredWindow 路径。

// 辅助函数：从包含透明度的十六进制颜色字符串创建 D2D1_COLOR_F
auto hex_with_alpha_to_color_f(const std::string& hex_color) -> D2D1_COLOR_F {
  std::string color_str = hex_color;
  if (color_str.starts_with("#")) {
    color_str = color_str.substr(1);
  }

  float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

  if (color_str.length() == 8) {
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
    a = std::stoi(color_str.substr(6, 2), nullptr, 16) / 255.0f;
  } else if (color_str.length() == 6) {
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
  }

  return D2D1::ColorF(r, g, b, a);
}

auto create_brush_from_hex(ID2D1DeviceContext6* target, const std::string& hex_color,
                           wil::com_ptr<ID2D1SolidColorBrush>& brush) -> bool {
  return target && SUCCEEDED(target->CreateSolidColorBrush(hex_with_alpha_to_color_f(hex_color),
                                                           brush.put()));
}

auto create_all_brushes_simple(Core::State::AppState& state, UI::FloatingWindow::RenderContext& d2d)
    -> bool {
  const auto& colors = state.settings->raw.ui.floating_window_colors;

  return create_brush_from_hex(d2d.device_context.get(), colors.background, d2d.background_brush) &&
         create_brush_from_hex(d2d.device_context.get(), colors.separator, d2d.separator_brush) &&
         create_brush_from_hex(d2d.device_context.get(), colors.text, d2d.text_brush) &&
         create_brush_from_hex(d2d.device_context.get(), colors.indicator, d2d.indicator_brush) &&
         create_brush_from_hex(d2d.device_context.get(), kRecordingIndicatorColor,
                               d2d.recording_indicator_brush) &&
         create_brush_from_hex(d2d.device_context.get(), colors.title_bar, d2d.title_brush) &&
         create_brush_from_hex(d2d.device_context.get(), colors.hover, d2d.hover_brush) &&
         create_brush_from_hex(d2d.device_context.get(), colors.scroll_indicator,
                               d2d.scroll_indicator_brush);
}

auto clear_text_caches(UI::FloatingWindow::RenderContext& d2d) -> void {
  d2d.adjusted_text_formats.clear();
  d2d.text_measure_cache.clear();
}

// 画刷仍按固定槽位缓存，保持和原先 painter 的调用方式一致，
// 避免为了切换后端而扩散到上层绘制逻辑。
auto release_brushes(UI::FloatingWindow::RenderContext& d2d) -> void {
  d2d.background_brush.reset();
  d2d.title_brush.reset();
  d2d.separator_brush.reset();
  d2d.text_brush.reset();
  d2d.indicator_brush.reset();
  d2d.recording_indicator_brush.reset();
  d2d.hover_brush.reset();
  d2d.scroll_indicator_brush.reset();
}

// target bitmap 来自 swap chain 当前 back buffer。
// resize 或重建时必须先从 device context 上解绑旧 target，再释放位图。
auto release_target_bitmap(UI::FloatingWindow::RenderContext& d2d) -> void {
  if (d2d.device_context) {
    d2d.device_context->SetTarget(nullptr);
  }
  d2d.target_bitmap.reset();
}

auto get_client_size(HWND hwnd) -> SIZE {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  return {rc.right - rc.left, rc.bottom - rc.top};
}

// D2D/DirectWrite 工厂在浮窗生命周期内复用；它们负责 2D 绘制和文本排版。
auto create_factories(UI::FloatingWindow::RenderContext& d2d) -> bool {
  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7),
                                 nullptr, reinterpret_cast<void**>(d2d.factory.put()));
  if (FAILED(hr)) {
    return false;
  }

  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
                           reinterpret_cast<IUnknown**>(d2d.write_factory.put()));
  return SUCCEEDED(hr);
}

// composition swap chain 需要 BGRA 的 D3D11 设备，D2D 也依赖这台设备创建自己的 device。
auto create_d3d_device(UI::FloatingWindow::RenderContext& d2d) -> bool {
  UINT create_device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
  create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  return SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                     create_device_flags, nullptr, 0, D3D11_SDK_VERSION,
                                     d2d.d3d_device.put(), nullptr, d2d.d3d_device_context.put()));
}

// D2D device context 才是后续 painter 的真正绘制目标。
auto create_d2d_device_context(UI::FloatingWindow::RenderContext& d2d) -> bool {
  wil::com_ptr<IDXGIDevice> dxgi_device;
  if (FAILED(d2d.d3d_device->QueryInterface(IID_PPV_ARGS(dxgi_device.put()))) || !dxgi_device) {
    return false;
  }

  const HRESULT device_hr = d2d.factory->CreateDevice(dxgi_device.get(), d2d.d2d_device.put());
  if (FAILED(device_hr) || !d2d.d2d_device) {
    return false;
  }

  wil::com_ptr<ID2D1DeviceContext> base_context;
  const HRESULT context_hr =
      d2d.d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, base_context.put());
  if (FAILED(context_hr) || !base_context) {
    return false;
  }

  const HRESULT query_hr = base_context->QueryInterface(IID_PPV_ARGS(d2d.device_context.put()));
  if (FAILED(query_hr) || !d2d.device_context) {
    return false;
  }

  d2d.device_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
  d2d.device_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  return true;
}

// 浮窗是透明 popup，不再自己持有一张 CPU DIB。
// swap chain 直接作为 DirectComposition visual 的内容，由 DWM 负责合成。
auto create_swap_chain(UI::FloatingWindow::RenderContext& d2d, const SIZE& size) -> bool {
  wil::com_ptr<IDXGIFactory2> dxgi_factory;
  if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(dxgi_factory.put()))) || !dxgi_factory) {
    return false;
  }

  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = static_cast<UINT>(size.cx);
  desc.Height = static_cast<UINT>(size.cy);
  desc.Format = kSurfaceFormat;
  desc.Stereo = FALSE;
  desc.SampleDesc = {1, 0};
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

  const HRESULT hr = dxgi_factory->CreateSwapChainForComposition(d2d.d3d_device.get(), &desc,
                                                                 nullptr, d2d.swap_chain.put());
  return SUCCEEDED(hr) && d2d.swap_chain;
}

// DirectComposition tree 只需要一层 root visual：
// visual.content = swap chain，target.root = visual。
// 这样浮窗仍然是普通 Win32 窗口，但像素内容由 composition 路径提交。
auto create_composition_tree(UI::FloatingWindow::RenderContext& d2d, HWND hwnd) -> bool {
  wil::com_ptr<IDXGIDevice> dxgi_device;
  if (FAILED(d2d.d3d_device->QueryInterface(IID_PPV_ARGS(dxgi_device.put()))) || !dxgi_device) {
    return false;
  }

  const HRESULT device_hr =
      DCompositionCreateDevice(dxgi_device.get(), IID_PPV_ARGS(d2d.composition_device.put()));
  if (FAILED(device_hr) || !d2d.composition_device) {
    return false;
  }

  if (FAILED(
          d2d.composition_device->CreateTargetForHwnd(hwnd, TRUE, d2d.composition_target.put())) ||
      !d2d.composition_target) {
    return false;
  }

  if (FAILED(d2d.composition_device->CreateVisual(d2d.composition_visual.put())) ||
      !d2d.composition_visual) {
    return false;
  }

  if (FAILED(d2d.composition_visual->SetContent(d2d.swap_chain.get())) ||
      FAILED(d2d.composition_target->SetRoot(d2d.composition_visual.get())) ||
      FAILED(d2d.composition_device->Commit())) {
    return false;
  }

  return true;
}

// 每次 resize 或 target 丢失后，都要重新从 back buffer 包一层 D2D bitmap，
// 然后把它设成当前 device context 的 target。
auto create_target_bitmap(UI::FloatingWindow::RenderContext& d2d, const SIZE& size) -> bool {
  release_target_bitmap(d2d);

  wil::com_ptr<IDXGISurface> dxgi_surface;
  if (FAILED(d2d.swap_chain->GetBuffer(0, IID_PPV_ARGS(dxgi_surface.put()))) || !dxgi_surface) {
    return false;
  }

  D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(kSurfaceFormat, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
  const HRESULT hr = d2d.device_context->CreateBitmapFromDxgiSurface(
      dxgi_surface.get(), &properties, d2d.target_bitmap.put());
  if (FAILED(hr) || !d2d.target_bitmap) {
    return false;
  }

  d2d.device_context->SetTarget(d2d.target_bitmap.get());
  d2d.surface_size = size;
  return true;
}

// 辅助函数：测量文本宽度
auto measure_text_width(const std::wstring& text, IDWriteTextFormat* text_format,
                        IDWriteFactory7* write_factory) -> float {
  if (text.empty() || !text_format || !write_factory) {
    return 0.0f;
  }

  wil::com_ptr<IDWriteTextLayout> text_layout;
  const HRESULT hr =
      write_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()), text_format,
                                      10000.0f, 10000.0f, text_layout.put());

  if (FAILED(hr) || !text_layout) {
    return 0.0f;
  }

  DWRITE_TEXT_METRICS metrics{};
  const HRESULT metrics_hr = text_layout->GetMetrics(&metrics);
  return SUCCEEDED(metrics_hr) ? metrics.width : 0.0f;
}

// 更新所有画刷颜色
auto update_all_brush_colors(Core::State::AppState& state) -> void {
  const auto& colors = state.settings->raw.ui.floating_window_colors;
  auto& d2d = state.floating_window->d2d_context;

  if (d2d.background_brush) {
    d2d.background_brush->SetColor(hex_with_alpha_to_color_f(colors.background));
  }
  if (d2d.title_brush) {
    d2d.title_brush->SetColor(hex_with_alpha_to_color_f(colors.title_bar));
  }
  if (d2d.separator_brush) {
    d2d.separator_brush->SetColor(hex_with_alpha_to_color_f(colors.separator));
  }
  if (d2d.text_brush) {
    d2d.text_brush->SetColor(hex_with_alpha_to_color_f(colors.text));
  }
  if (d2d.indicator_brush) {
    d2d.indicator_brush->SetColor(hex_with_alpha_to_color_f(colors.indicator));
  }
  if (d2d.recording_indicator_brush) {
    d2d.recording_indicator_brush->SetColor(hex_with_alpha_to_color_f(kRecordingIndicatorColor));
  }
  if (d2d.hover_brush) {
    d2d.hover_brush->SetColor(hex_with_alpha_to_color_f(colors.hover));
  }
  if (d2d.scroll_indicator_brush) {
    d2d.scroll_indicator_brush->SetColor(hex_with_alpha_to_color_f(colors.scroll_indicator));
  }
}

// 创建具有指定字体大小的文本格式
auto create_text_format_with_size(IDWriteFactory7* write_factory, float font_size)
    -> wil::com_ptr<IDWriteTextFormat> {
  if (!write_factory) {
    return {};
  }

  wil::com_ptr<IDWriteTextFormat> text_format;
  const HRESULT hr = write_factory->CreateTextFormat(
      L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, font_size, L"zh-CN", text_format.put());

  if (FAILED(hr) || !text_format) {
    return {};
  }

  text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  return text_format;
}

// 初始化Direct2D资源
auto initialize_d2d(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& d2d = state.floating_window->d2d_context;
  const SIZE size = get_client_size(hwnd);
  if (size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  cleanup_d2d(state);

  // 初始化按“工厂 -> D3D -> D2D context -> swap chain -> composition tree -> target bitmap”
  // 的顺序推进；任一步失败都整体回滚，保持下次可从干净状态重建。
  if (!create_factories(d2d) || !create_d3d_device(d2d) || !create_d2d_device_context(d2d) ||
      !create_swap_chain(d2d, size) || !create_composition_tree(d2d, hwnd) ||
      !create_target_bitmap(d2d, size) || !create_all_brushes_simple(state, d2d)) {
    cleanup_d2d(state);
    return false;
  }

  d2d.text_format = create_text_format_with_size(d2d.write_factory.get(),
                                                 state.floating_window->layout.font_size);
  if (!d2d.text_format) {
    cleanup_d2d(state);
    return false;
  }

  d2d.is_initialized = true;
  return true;
}

// 清理Direct2D资源
auto cleanup_d2d(Core::State::AppState& state) -> void {
  auto& d2d = state.floating_window->d2d_context;

  // 先清掉依赖 device context 的缓存和 brush，再按 target -> device -> composition -> D3D
  // 的反向顺序释放，避免留下悬空的 target 绑定。
  clear_text_caches(d2d);
  release_brushes(d2d);
  d2d.text_format.reset();
  release_target_bitmap(d2d);

  d2d.device_context.reset();
  d2d.d2d_device.reset();
  d2d.write_factory.reset();
  d2d.factory.reset();

  d2d.composition_visual.reset();
  d2d.composition_target.reset();
  d2d.composition_device.reset();
  d2d.swap_chain.reset();
  d2d.d3d_device_context.reset();
  d2d.d3d_device.reset();

  d2d.surface_size = {0, 0};
  d2d.is_initialized = false;
  d2d.is_rendering = false;
}

// 调整渲染目标大小
auto resize_d2d(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& d2d = state.floating_window->d2d_context;
  if (!d2d.is_initialized || !d2d.swap_chain || !d2d.device_context || new_size.cx <= 0 ||
      new_size.cy <= 0) {
    return false;
  }

  if (d2d.surface_size.cx == new_size.cx && d2d.surface_size.cy == new_size.cy) {
    return true;
  }

  release_target_bitmap(d2d);

  // composition swap chain resize 只需要重分配 back buffer，
  // 上层 painter 和 brush 都可以原样复用。
  if (FAILED(d2d.swap_chain->ResizeBuffers(0, static_cast<UINT>(new_size.cx),
                                           static_cast<UINT>(new_size.cy), kSurfaceFormat, 0)) ||
      !create_target_bitmap(d2d, new_size)) {
    return false;
  }

  return true;
}

// 更新文本格式（DPI变化时）
auto update_text_format_if_needed(Core::State::AppState& state) -> bool {
  auto& d2d = state.floating_window->d2d_context;
  auto& layout = state.floating_window->layout;

  if (!d2d.needs_font_update || !d2d.write_factory) {
    return true;
  }

  // 字体更新只重建 text format 和测量缓存，不触碰底层 composition / swap chain 资源。
  d2d.text_format.reset();
  d2d.text_format = create_text_format_with_size(d2d.write_factory.get(), layout.font_size);
  if (!d2d.text_format) {
    return false;
  }

  clear_text_caches(d2d);
  d2d.needs_font_update = false;
  return true;
}

}  // namespace UI::FloatingWindow::D2DContext
