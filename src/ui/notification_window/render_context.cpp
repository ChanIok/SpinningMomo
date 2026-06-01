module;

module UI.NotificationWindow.RenderContext;

import Core.State;
import UI.NotificationWindow.State;
import UI.NotificationWindow.Types;
import UI.SharedRenderResources;
import UI.SharedRenderResources.State;
import <d2d1_3.h>;
import <d3d11.h>;
import <dcomp.h>;
import <dwrite_3.h>;
import <dxgi1_2.h>;
import <wil/com.h>;
import <windows.h>;

namespace UI::NotificationWindow::RenderContext {

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

auto create_text_format(IDWriteFactory7* write_factory, float font_size,
                        DWRITE_TEXT_ALIGNMENT text_alignment,
                        DWRITE_PARAGRAPH_ALIGNMENT paragraph_alignment,
                        DWRITE_WORD_WRAPPING word_wrapping) -> wil::com_ptr<IDWriteTextFormat> {
  if (!write_factory) {
    return {};
  }

  wil::com_ptr<IDWriteTextFormat> format;
  const HRESULT hr = write_factory->CreateTextFormat(
      L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, font_size, L"zh-CN", format.put());
  if (FAILED(hr) || !format) {
    return {};
  }

  format->SetTextAlignment(text_alignment);
  format->SetParagraphAlignment(paragraph_alignment);
  format->SetWordWrapping(word_wrapping);
  return format;
}

auto release_text_formats(NotificationWindow::RenderResources& render_resources) -> void {
  render_resources.title_text_format.reset();
  render_resources.message_text_format.reset();
  render_resources.button_text_format.reset();
}

auto release_brushes(NotificationWindow::RenderResources& render_resources) -> void {
  render_resources.fill_brush.reset();
  render_resources.stroke_brush.reset();
  render_resources.text_brush.reset();
}

auto release_target_bitmap(NotificationWindow::RenderResources& render_resources) -> void {
  if (render_resources.device_context) {
    render_resources.device_context->SetTarget(nullptr);
  }
  render_resources.target_bitmap.reset();
}

auto create_device_context(ID2D1Device* shared_device,
                           NotificationWindow::RenderResources& render_resources) -> bool {
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

auto create_swap_chain(ID3D11Device* shared_d3d_device,
                       NotificationWindow::RenderResources& render_resources, const SIZE& size)
    -> bool {
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

auto create_composition_tree(ID3D11Device* shared_d3d_device,
                             NotificationWindow::RenderResources& render_resources, HWND hwnd)
    -> bool {
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

auto create_target_bitmap(NotificationWindow::RenderResources& render_resources, const SIZE& size)
    -> bool {
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

auto create_brushes(NotificationWindow::RenderResources& render_resources) -> bool {
  if (!render_resources.device_context) {
    return false;
  }

  return SUCCEEDED(render_resources.device_context->CreateSolidColorBrush(
             D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), render_resources.fill_brush.put())) &&
         SUCCEEDED(render_resources.device_context->CreateSolidColorBrush(
             D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), render_resources.stroke_brush.put())) &&
         SUCCEEDED(render_resources.device_context->CreateSolidColorBrush(
             D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), render_resources.text_brush.put()));
}

auto ensure_text_formats(Core::State::AppState& state,
                         NotificationWindow::RenderResources& render_resources, int dpi) -> bool {
  if (render_resources.title_text_format && render_resources.message_text_format &&
      render_resources.button_text_format && render_resources.dpi == dpi) {
    return true;
  }

  release_text_formats(render_resources);

  auto* write_factory = shared_resources(state).write_factory.get();
  const auto scale_for_dpi = [dpi](int value) -> float {
    return static_cast<float>(MulDiv(value, dpi, 96));
  };

  render_resources.title_text_format = create_text_format(
      write_factory, scale_for_dpi(NotificationWindow::BASE_TITLE_FONT_SIZE),
      DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
  render_resources.message_text_format =
      create_text_format(write_factory, scale_for_dpi(NotificationWindow::BASE_FONT_SIZE),
                         DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
                         DWRITE_WORD_WRAPPING_CHARACTER);
  render_resources.button_text_format =
      create_text_format(write_factory, scale_for_dpi(NotificationWindow::BASE_FONT_SIZE),
                         DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                         DWRITE_WORD_WRAPPING_NO_WRAP);

  render_resources.dpi = dpi;
  return render_resources.title_text_format && render_resources.message_text_format &&
         render_resources.button_text_format;
}

auto reset_render_context(NotificationWindow::RenderResources& render_resources) -> void {
  release_text_formats(render_resources);
  release_brushes(render_resources);
  release_target_bitmap(render_resources);
  render_resources.device_context.reset();
  render_resources.composition_visual.reset();
  render_resources.composition_target.reset();
  render_resources.swap_chain.reset();
  render_resources.surface_size = {0, 0};
  render_resources.is_ready = false;
  render_resources.is_rendering = false;
  render_resources.dpi = 96;
}

auto ensure_render_context(Core::State::AppState& state) -> bool {
  auto& window_state = *state.notification_window;
  auto& render_resources = window_state.render_resources;
  if (!window_state.host_hwnd) {
    return false;
  }
  if (!UI::SharedRenderResources::ensure_initialized(state)) {
    return false;
  }

  const SIZE size = get_client_size(window_state.host_hwnd);
  if (size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  auto& shared = shared_resources(state);
  if (!render_resources.is_ready) {
    reset_render_context(render_resources);
    if (!create_device_context(shared.d2d_device.get(), render_resources) ||
        !create_swap_chain(shared.d3d_device.get(), render_resources, size) ||
        !create_composition_tree(shared.d3d_device.get(), render_resources,
                                 window_state.host_hwnd) ||
        !create_target_bitmap(render_resources, size) || !create_brushes(render_resources)) {
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

  if (!ensure_text_formats(state, render_resources,
                           static_cast<int>(GetDpiForWindow(window_state.host_hwnd)))) {
    reset_render_context(render_resources);
    return false;
  }

  render_resources.is_ready = true;
  return true;
}

auto cleanup_render_context(Core::State::AppState& state) -> void {
  reset_render_context(state.notification_window->render_resources);
}

auto resize_render_context(Core::State::AppState& state, const SIZE& new_size) -> bool {
  auto& render_resources = state.notification_window->render_resources;
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

}  // namespace UI::NotificationWindow::RenderContext
