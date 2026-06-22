module;

module UI.PhotographyPanel.RenderContext;

import Core.State;
import UI.PhotographyPanel.State;
import UI.SharedRenderResources;
import UI.SharedRenderResources.State;
import UI.SharedTheme;
import UI.FloatingWindow.RenderContext;
import UI.FloatingWindow.State;
import <d2d1_3.h>;
import <d3d11.h>;
import <dcomp.h>;
import <dwrite_3.h>;
import <dxgi1_2.h>;
import <wil/com.h>;
import <windows.h>;

namespace UI::PhotographyPanel::RenderContext {

constexpr DXGI_FORMAT kSurfaceFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

auto shared_resources(Core::State::AppState& state)
    -> UI::SharedRenderResources::State::SharedRenderResourcesState& {
  return *state.shared_render_resources;
}

auto get_client_size(HWND hwnd) -> SIZE {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  return {rc.right - rc.left, rc.bottom - rc.top};
}

auto create_device_context(ID2D1Device* shared_device, State::RenderResources& render_resources)
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

  if (FAILED(base_context->QueryInterface(IID_PPV_ARGS(render_resources.device_context.put()))) ||
      !render_resources.device_context) {
    return false;
  }

  render_resources.device_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
  render_resources.device_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  return true;
}

auto create_swap_chain(ID3D11Device* shared_d3d_device, State::RenderResources& render_resources,
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

  return SUCCEEDED(dxgi_factory->CreateSwapChainForComposition(
             shared_d3d_device, &desc, nullptr, render_resources.swap_chain.put())) &&
         render_resources.swap_chain;
}

// 建立 DirectComposition 树：hwnd → target → visual → swap chain，实现亚像素透明
auto create_composition_tree(ID3D11Device* shared_d3d_device,
                             State::RenderResources& render_resources, HWND hwnd) -> bool {
  if (!shared_d3d_device || !render_resources.swap_chain) {
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

  if (FAILED(composition_device->CreateTargetForHwnd(hwnd, TRUE,
                                                     render_resources.composition_target.put())) ||
      !render_resources.composition_target) {
    return false;
  }

  if (FAILED(composition_device->CreateVisual(render_resources.composition_visual.put())) ||
      !render_resources.composition_visual) {
    return false;
  }

  return SUCCEEDED(
             render_resources.composition_visual->SetContent(render_resources.swap_chain.get())) &&
         SUCCEEDED(render_resources.composition_target->SetRoot(
             render_resources.composition_visual.get())) &&
         SUCCEEDED(composition_device->Commit());
}

auto release_target_bitmap(State::RenderResources& render_resources) -> void {
  if (render_resources.device_context) {
    render_resources.device_context->SetTarget(nullptr);
  }
  render_resources.target_bitmap.reset();
}

// 从 swap chain 获取 back buffer 并绑定为 D2D 绘制目标
auto create_target_bitmap(State::RenderResources& render_resources, const SIZE& size) -> bool {
  release_target_bitmap(render_resources);

  wil::com_ptr<IDXGISurface> dxgi_surface;
  if (FAILED(render_resources.swap_chain->GetBuffer(0, IID_PPV_ARGS(dxgi_surface.put()))) ||
      !dxgi_surface) {
    return false;
  }

  D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(kSurfaceFormat, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
  if (FAILED(render_resources.device_context->CreateBitmapFromDxgiSurface(
          dxgi_surface.get(), &properties, render_resources.target_bitmap.put())) ||
      !render_resources.target_bitmap) {
    return false;
  }

  render_resources.device_context->SetTarget(render_resources.target_bitmap.get());
  render_resources.surface_size = size;
  return true;
}

// 按浮窗 token 创建面板画刷，让摄影面板和主浮窗共用同一套配色语义
auto create_brushes(Core::State::AppState& state, State::RenderResources& render_resources)
    -> bool {
  auto* context = render_resources.device_context.get();
  if (!context) {
    return false;
  }

  // 这里只映射摄影面板真正需要的槽位，避免为了一致性强行引入无关样式
  const auto colors = UI::SharedTheme::resolve_floating_window_theme_colors(state);
  return SUCCEEDED(context->CreateSolidColorBrush(colors.background,
                                                  render_resources.background_brush.put())) &&
         SUCCEEDED(context->CreateSolidColorBrush(colors.title_bar,
                                                  render_resources.title_brush.put())) &&
         SUCCEEDED(
             context->CreateSolidColorBrush(colors.text, render_resources.text_brush.put())) &&
         SUCCEEDED(context->CreateSolidColorBrush(colors.separator,
                                                  render_resources.track_brush.put())) &&
         SUCCEEDED(
             context->CreateSolidColorBrush(colors.indicator, render_resources.knob_brush.put()));
}

// 释放所有 D3D/D2D 资源并重置状态标记
auto reset_render_context(State::RenderResources& render_resources) -> void {
  render_resources.text_format.reset();
  render_resources.background_brush.reset();
  render_resources.title_brush.reset();
  render_resources.text_brush.reset();
  render_resources.track_brush.reset();
  render_resources.knob_brush.reset();
  release_target_bitmap(render_resources);
  render_resources.device_context.reset();
  render_resources.composition_visual.reset();
  render_resources.composition_target.reset();
  render_resources.swap_chain.reset();
  render_resources.surface_size = {0, 0};
  render_resources.is_ready = false;
  render_resources.is_rendering = false;
}

// 确保 D3D 渲染管线就绪：首次调用时完整初始化，后续仅在窗口大小变化时 resize
auto ensure_render_context(Core::State::AppState& state) -> bool {
  auto& panel = *state.photography_panel;
  auto& render_resources = panel.render_resources;
  if (!panel.hwnd) {
    return false;
  }
  if (!UI::SharedRenderResources::ensure_initialized(state)) {
    return false;
  }

  const SIZE size = get_client_size(panel.hwnd);
  if (size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  auto& shared = shared_resources(state);
  if (!render_resources.is_ready) {
    // 首次创建时一次性把 surface、画刷和字体都建好，避免首帧出现半初始化状态
    reset_render_context(render_resources);
    if (!create_device_context(shared.d2d_device.get(), render_resources) ||
        !create_swap_chain(shared.d3d_device.get(), render_resources, size) ||
        !create_composition_tree(shared.d3d_device.get(), render_resources, panel.hwnd) ||
        !create_target_bitmap(render_resources, size) || !create_brushes(state, render_resources)) {
      reset_render_context(render_resources);
      return false;
    }

    if (!update_text_format(state)) {
      reset_render_context(render_resources);
      return false;
    }
  } else if (render_resources.surface_size.cx != size.cx ||
             render_resources.surface_size.cy != size.cy) {
    if (!resize_render_context(state, size)) {
      reset_render_context(render_resources);
      return false;
    }
  }

  render_resources.is_ready = true;
  return true;
}

// 窗口大小变化时重建 back buffer，避免拉伸或裁剪
auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& render_resources = state.photography_panel->render_resources;
  if (!render_resources.is_ready || !render_resources.swap_chain ||
      !render_resources.device_context || new_size.cx <= 0 || new_size.cy <= 0) {
    return false;
  }

  if (render_resources.surface_size.cx == new_size.cx &&
      render_resources.surface_size.cy == new_size.cy) {
    return true;
  }

  release_target_bitmap(render_resources);
  if (FAILED(render_resources.swap_chain->ResizeBuffers(
          0, static_cast<UINT>(new_size.cx), static_cast<UINT>(new_size.cy), kSurfaceFormat, 0)) ||
      !create_target_bitmap(render_resources, new_size)) {
    return false;
  }

  return true;
}

auto cleanup_render_context(Core::State::AppState& state) -> void {
  if (!state.photography_panel) {
    return;
  }
  reset_render_context(state.photography_panel->render_resources);
}

// 设置变化时只改现有 brush 的颜色，避免为了换肤去重建整个渲染后端
auto update_theme_brushes(Core::State::AppState& state) -> void {
  auto& render_resources = state.photography_panel->render_resources;
  if (!render_resources.device_context) {
    return;
  }

  const auto colors = UI::SharedTheme::resolve_floating_window_theme_colors(state);
  if (render_resources.background_brush) {
    render_resources.background_brush->SetColor(colors.background);
  }
  if (render_resources.title_brush) {
    render_resources.title_brush->SetColor(colors.title_bar);
  }
  if (render_resources.text_brush) {
    render_resources.text_brush->SetColor(colors.text);
  }
  if (render_resources.track_brush) {
    render_resources.track_brush->SetColor(colors.separator);
  }
  if (render_resources.knob_brush) {
    render_resources.knob_brush->SetColor(colors.indicator);
  }
}

// 面板直接沿用浮窗字号，保证标题和内容行的文字密度始终一致
auto update_text_format(Core::State::AppState& state) -> bool {
  auto& render_resources = state.photography_panel->render_resources;
  if (!UI::SharedRenderResources::ensure_initialized(state)) {
    return false;
  }

  render_resources.text_format = UI::FloatingWindow::RenderContext::create_text_format_with_size(
      shared_resources(state).write_factory.get(), state.floating_window->layout.font_size);
  return static_cast<bool>(render_resources.text_format);
}

}  // namespace UI::PhotographyPanel::RenderContext
