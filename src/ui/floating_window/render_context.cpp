module;

module UI.FloatingWindow.RenderContext;

import Core.State;
import UI.FloatingWindow.State;
import UI.FloatingWindow.Types;
import UI.SharedRenderResources;
import UI.SharedRenderResources.State;
import UI.SharedTheme;
import Features.Settings.State;
import <d2d1_3.h>;
import <d3d11.h>;
import <dcomp.h>;
import <dwrite_3.h>;
import <dxgi1_2.h>;
import <wil/com.h>;
import <windows.h>;

namespace UI::FloatingWindow::RenderContext {

constexpr const char* kRecordingIndicatorColor = "#ED4C4CFF";
constexpr DXGI_FORMAT kSurfaceFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

// 浮窗现在只保留自己的窗口级 surface；
// D3D11 / D2D factory / DWrite factory 统一来自共享设备级状态。

auto recording_indicator_color() -> D2D1_COLOR_F {
  return D2D1::ColorF(0.9294f, 0.2980f, 0.2980f, 1.0f);
}

auto shared_resources(Core::State::AppState& state)
    -> UI::SharedRenderResources::State::SharedRenderResourcesState& {
  return *state.shared_render_resources;
}

auto create_brush_from_color(ID2D1DeviceContext6* target, const D2D1_COLOR_F& color,
                             wil::com_ptr<ID2D1SolidColorBrush>& brush) -> bool {
  return target && SUCCEEDED(target->CreateSolidColorBrush(color, brush.put()));
}

// 用共享 theme token 创建浮窗画刷，让其他窗口也能复用同一套颜色来源
auto create_all_brushes_simple(Core::State::AppState& state,
                               UI::FloatingWindow::RenderResources& d2d) -> bool {
  const auto colors = UI::SharedTheme::resolve_floating_window_theme_colors(state);

  return create_brush_from_color(d2d.device_context.get(), colors.background,
                                 d2d.background_brush) &&
         create_brush_from_color(d2d.device_context.get(), colors.separator, d2d.separator_brush) &&
         create_brush_from_color(d2d.device_context.get(), colors.text, d2d.text_brush) &&
         create_brush_from_color(d2d.device_context.get(), colors.indicator, d2d.indicator_brush) &&
         create_brush_from_color(d2d.device_context.get(), recording_indicator_color(),
                                 d2d.recording_indicator_brush) &&
         create_brush_from_color(d2d.device_context.get(), colors.title_bar, d2d.title_brush) &&
         create_brush_from_color(d2d.device_context.get(), colors.hover, d2d.hover_brush) &&
         create_brush_from_color(d2d.device_context.get(), colors.scroll_indicator,
                                 d2d.scroll_indicator_brush);
}

auto clear_text_caches(UI::FloatingWindow::RenderResources& d2d) -> void {
  d2d.adjusted_text_formats.clear();
  d2d.text_measure_cache.clear();
}

// 画刷仍按固定槽位缓存，保持和原先 painter 的调用方式一致，
// 避免为了切换后端而扩散到上层绘制逻辑。
auto release_brushes(UI::FloatingWindow::RenderResources& d2d) -> void {
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
auto release_target_bitmap(UI::FloatingWindow::RenderResources& d2d) -> void {
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

auto create_device_context(ID2D1Device* shared_device, UI::FloatingWindow::RenderResources& d2d)
    -> bool {
  if (!shared_device) {
    return false;
  }

  wil::com_ptr<ID2D1DeviceContext> base_context;
  if (FAILED(shared_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                base_context.put())) ||
      !base_context) {
    return false;
  }

  if (FAILED(base_context->QueryInterface(IID_PPV_ARGS(d2d.device_context.put()))) ||
      !d2d.device_context) {
    return false;
  }

  d2d.device_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
  d2d.device_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  return true;
}

// 浮窗是透明 popup，不再自己持有一张 CPU DIB。
// swap chain 直接作为 DirectComposition visual 的内容，由 DWM 负责合成。
auto create_swap_chain(ID3D11Device* shared_d3d_device, UI::FloatingWindow::RenderResources& d2d,
                       const SIZE& size) -> bool {
  if (!shared_d3d_device) {
    return false;
  }

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

  return SUCCEEDED(dxgi_factory->CreateSwapChainForComposition(shared_d3d_device, &desc, nullptr,
                                                               d2d.swap_chain.put())) &&
         d2d.swap_chain;
}

// DirectComposition tree 只需要一层 root visual：
// visual.content = swap chain，target.root = visual。
auto create_composition_tree(ID3D11Device* shared_d3d_device,
                             UI::FloatingWindow::RenderResources& d2d, HWND hwnd) -> bool {
  if (!shared_d3d_device || !d2d.swap_chain) {
    return false;
  }

  wil::com_ptr<IDXGIDevice> dxgi_device;
  if (FAILED(shared_d3d_device->QueryInterface(IID_PPV_ARGS(dxgi_device.put()))) || !dxgi_device) {
    return false;
  }

  wil::com_ptr<IDCompositionDevice> composition_device;
  if (FAILED(DCompositionCreateDevice(dxgi_device.get(), IID_PPV_ARGS(composition_device.put()))) ||
      !composition_device) {
    return false;
  }

  if (FAILED(composition_device->CreateTargetForHwnd(hwnd, TRUE, d2d.composition_target.put())) ||
      !d2d.composition_target) {
    return false;
  }

  if (FAILED(composition_device->CreateVisual(d2d.composition_visual.put())) ||
      !d2d.composition_visual) {
    return false;
  }

  return SUCCEEDED(d2d.composition_visual->SetContent(d2d.swap_chain.get())) &&
         SUCCEEDED(d2d.composition_target->SetRoot(d2d.composition_visual.get())) &&
         SUCCEEDED(composition_device->Commit());
}

// 每次 resize 或 target 丢失后，都要重新从 back buffer 包一层 D2D bitmap，
// 然后把它设成当前 device context 的 target。
auto create_target_bitmap(UI::FloatingWindow::RenderResources& d2d, const SIZE& size) -> bool {
  release_target_bitmap(d2d);

  wil::com_ptr<IDXGISurface> dxgi_surface;
  if (FAILED(d2d.swap_chain->GetBuffer(0, IID_PPV_ARGS(dxgi_surface.put()))) || !dxgi_surface) {
    return false;
  }

  D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(kSurfaceFormat, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
  if (FAILED(d2d.device_context->CreateBitmapFromDxgiSurface(dxgi_surface.get(), &properties,
                                                             d2d.target_bitmap.put())) ||
      !d2d.target_bitmap) {
    return false;
  }

  d2d.device_context->SetTarget(d2d.target_bitmap.get());
  d2d.surface_size = size;
  return true;
}

auto measure_text_width(const std::wstring& text, IDWriteTextFormat* text_format,
                        IDWriteFactory7* write_factory) -> float {
  if (text.empty() || !text_format || !write_factory) {
    return 0.0f;
  }

  wil::com_ptr<IDWriteTextLayout> text_layout;
  if (FAILED(write_factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()),
                                             text_format, 10000.0f, 10000.0f, text_layout.put())) ||
      !text_layout) {
    return 0.0f;
  }

  DWRITE_TEXT_METRICS metrics{};
  return SUCCEEDED(text_layout->GetMetrics(&metrics)) ? metrics.width : 0.0f;
}

// 设置换肤时原地更新浮窗画刷，避免重建设备资源导致闪烁或状态抖动
auto update_all_brush_colors(Core::State::AppState& state) -> void {
  const auto colors = UI::SharedTheme::resolve_floating_window_theme_colors(state);
  auto& d2d = state.floating_window->render_resources;

  if (d2d.background_brush) {
    d2d.background_brush->SetColor(colors.background);
  }
  if (d2d.title_brush) {
    d2d.title_brush->SetColor(colors.title_bar);
  }
  if (d2d.separator_brush) {
    d2d.separator_brush->SetColor(colors.separator);
  }
  if (d2d.text_brush) {
    d2d.text_brush->SetColor(colors.text);
  }
  if (d2d.indicator_brush) {
    d2d.indicator_brush->SetColor(colors.indicator);
  }
  if (d2d.recording_indicator_brush) {
    d2d.recording_indicator_brush->SetColor(recording_indicator_color());
  }
  if (d2d.hover_brush) {
    d2d.hover_brush->SetColor(colors.hover);
  }
  if (d2d.scroll_indicator_brush) {
    d2d.scroll_indicator_brush->SetColor(colors.scroll_indicator);
  }
}

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

auto initialize_render_context(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& d2d = state.floating_window->render_resources;
  auto& shared = shared_resources(state);
  const SIZE size = get_client_size(hwnd);
  if (size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  cleanup_render_context(state);

  if (!UI::SharedRenderResources::ensure_initialized(state)) {
    return false;
  }

  // 初始化按“共享设备级资源 -> 窗口级 device context -> swap chain -> composition tree
  // -> target bitmap”的顺序推进；任一步失败都整体回滚。
  if (!create_device_context(shared.d2d_device.get(), d2d) ||
      !create_swap_chain(shared.d3d_device.get(), d2d, size) ||
      !create_composition_tree(shared.d3d_device.get(), d2d, hwnd) ||
      !create_target_bitmap(d2d, size) || !create_all_brushes_simple(state, d2d)) {
    cleanup_render_context(state);
    return false;
  }

  d2d.text_format = create_text_format_with_size(shared.write_factory.get(),
                                                 state.floating_window->layout.font_size);
  if (!d2d.text_format) {
    cleanup_render_context(state);
    return false;
  }

  d2d.is_initialized = true;
  return true;
}

auto cleanup_render_context(Core::State::AppState& state) -> void {
  auto& d2d = state.floating_window->render_resources;

  // 先清掉依赖 device context 的缓存和 brush，再按 target -> surface 的反向顺序释放，
  // 避免留下悬空的 target 绑定。
  clear_text_caches(d2d);
  release_brushes(d2d);
  d2d.text_format.reset();
  release_target_bitmap(d2d);

  d2d.device_context.reset();
  d2d.composition_visual.reset();
  d2d.composition_target.reset();
  d2d.swap_chain.reset();

  d2d.surface_size = {0, 0};
  d2d.is_initialized = false;
  d2d.is_rendering = false;
}

auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& d2d = state.floating_window->render_resources;
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

auto update_text_format_if_needed(Core::State::AppState& state) -> bool {
  auto& d2d = state.floating_window->render_resources;
  auto& layout = state.floating_window->layout;
  auto& shared = shared_resources(state);

  if (!d2d.needs_font_update) {
    return true;
  }

  if (!shared.is_initialized && !UI::SharedRenderResources::ensure_initialized(state)) {
    return false;
  }

  // 字体更新只重建 text format 和测量缓存，不触碰底层 composition / swap chain 资源。
  d2d.text_format.reset();
  d2d.text_format = create_text_format_with_size(shared.write_factory.get(), layout.font_size);
  if (!d2d.text_format) {
    return false;
  }

  clear_text_caches(d2d);
  d2d.needs_font_update = false;
  return true;
}

}  // namespace UI::FloatingWindow::RenderContext
