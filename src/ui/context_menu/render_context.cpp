module;

module UI.ContextMenu.RenderContext;

import Core.State;
import UI.SharedRenderResources;
import UI.SharedRenderResources.State;
import UI.ContextMenu.State;
import Utils.Logger;
import Features.Settings.State;
import <d2d1_3.h>;
import <d3d11.h>;
import <dcomp.h>;
import <dwrite_3.h>;
import <dxgi1_2.h>;
import <wil/com.h>;
import <windows.h>;

namespace UI::ContextMenu::RenderContext {

constexpr DXGI_FORMAT kSurfaceFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

auto hex_with_alpha_to_color_f(const std::string& hex_color) -> D2D1_COLOR_F {
  std::string color_str = hex_color;
  if (color_str.starts_with("#")) {
    color_str = color_str.substr(1);
  }

  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;

  if (color_str.length() == 8) {
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
    a = std::stoi(color_str.substr(6, 2), nullptr, 16) / 255.0f;
  } else if (color_str.length() >= 6) {
    r = std::stoi(color_str.substr(0, 2), nullptr, 16) / 255.0f;
    g = std::stoi(color_str.substr(2, 2), nullptr, 16) / 255.0f;
    b = std::stoi(color_str.substr(4, 2), nullptr, 16) / 255.0f;
  }

  return D2D1::ColorF(r, g, b, a);
}

auto force_opaque_hex_color(std::string hex_color) -> std::string {
  if (hex_color.empty()) {
    return hex_color;
  }

  const bool has_hash = hex_color.starts_with("#");
  std::string color = has_hash ? hex_color.substr(1) : hex_color;

  if (color.length() >= 8) {
    color = color.substr(0, 6) + "FF";
  } else if (color.length() == 6) {
    color += "FF";
  }

  return has_hash ? "#" + color : color;
}

auto shared_resources(Core::State::AppState& state)
    -> UI::SharedRenderResources::State::SharedRenderResourcesState& {
  return *state.shared_render_resources;
}

auto create_brush_from_hex(ID2D1DeviceContext6* target, const std::string& hex_color,
                           wil::com_ptr<ID2D1SolidColorBrush>& brush) -> bool {
  return target && SUCCEEDED(target->CreateSolidColorBrush(hex_with_alpha_to_color_f(hex_color),
                                                           brush.put()));
}

auto release_brushes(State::RenderResources& render_resources) -> void {
  render_resources.background_brush.reset();
  render_resources.text_brush.reset();
  render_resources.separator_brush.reset();
  render_resources.hover_brush.reset();
  render_resources.indicator_brush.reset();
}

auto release_target_bitmap(State::RenderResources& render_resources) -> void {
  if (render_resources.device_context) {
    render_resources.device_context->SetTarget(nullptr);
  }
  render_resources.target_bitmap.reset();
}

auto cleanup_surface(State::RenderResources& render_resources) -> void {
  release_brushes(render_resources);
  release_target_bitmap(render_resources);
  render_resources.device_context.reset();
  render_resources.composition_visual.reset();
  render_resources.composition_target.reset();
  render_resources.swap_chain.reset();
  render_resources.surface_size = {0, 0};
  render_resources.is_ready = false;
}

auto create_brushes_for_surface(Core::State::AppState& state,
                                State::RenderResources& render_resources) -> bool {
  const auto& colors = state.settings->raw.ui.floating_window_colors;
  return create_brush_from_hex(render_resources.device_context.get(),
                               force_opaque_hex_color(colors.background),
                               render_resources.background_brush) &&
         create_brush_from_hex(render_resources.device_context.get(), colors.text,
                               render_resources.text_brush) &&
         create_brush_from_hex(render_resources.device_context.get(),
                               force_opaque_hex_color(colors.separator),
                               render_resources.separator_brush) &&
         create_brush_from_hex(render_resources.device_context.get(),
                               force_opaque_hex_color(colors.hover),
                               render_resources.hover_brush) &&
         create_brush_from_hex(render_resources.device_context.get(), colors.indicator,
                               render_resources.indicator_brush);
}

auto create_text_format(IDWriteFactory7* write_factory, float font_size)
    -> wil::com_ptr<IDWriteTextFormat> {
  if (!write_factory) {
    return {};
  }

  wil::com_ptr<IDWriteTextFormat> text_format;
  if (FAILED(write_factory->CreateTextFormat(L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                             DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                             font_size, L"zh-CN", text_format.put())) ||
      !text_format) {
    return {};
  }

  text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  return text_format;
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

auto initialize_surface(Core::State::AppState& state, State::RenderResources& render_resources,
                        HWND hwnd, const SIZE& size) -> bool {
  auto& shared = shared_resources(state);
  if (!UI::SharedRenderResources::ensure_initialized(state) || size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  if (render_resources.is_ready && render_resources.surface_size.cx == size.cx &&
      render_resources.surface_size.cy == size.cy) {
    return true;
  }

  cleanup_surface(render_resources);

  if (!create_device_context(shared.d2d_device.get(), render_resources) ||
      !create_swap_chain(shared.d3d_device.get(), render_resources, size) ||
      !create_composition_tree(shared.d3d_device.get(), render_resources, hwnd) ||
      !create_target_bitmap(render_resources, size) ||
      !create_brushes_for_surface(state, render_resources)) {
    cleanup_surface(render_resources);
    return false;
  }

  render_resources.is_ready = true;
  return true;
}

auto resize_surface(State::RenderResources& render_resources, const SIZE& new_size) -> bool {
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

auto get_client_size(HWND hwnd) -> SIZE {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  return {rc.right - rc.left, rc.bottom - rc.top};
}

auto initialize_text_format(Core::State::AppState& state) -> bool {
  auto& menu_state = *state.context_menu;
  if (!UI::SharedRenderResources::ensure_initialized(state)) {
    return false;
  }

  menu_state.text_format.reset();
  menu_state.text_format = create_text_format(shared_resources(state).write_factory.get(),
                                              static_cast<float>(menu_state.layout.font_size));
  return static_cast<bool>(menu_state.text_format);
}

auto initialize_context_menu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& menu_state = *state.context_menu;
  if (!menu_state.text_format && !initialize_text_format(state)) {
    return false;
  }

  return initialize_surface(state, menu_state.main_render_resources, hwnd, get_client_size(hwnd));
}

auto cleanup_context_menu(Core::State::AppState& state) -> void {
  cleanup_surface(state.context_menu->main_render_resources);
  state.context_menu->text_format.reset();
}

auto initialize_submenu(Core::State::AppState& state, HWND hwnd) -> bool {
  auto& menu_state = *state.context_menu;
  if (!menu_state.text_format && !initialize_text_format(state)) {
    return false;
  }

  return initialize_surface(state, menu_state.submenu_render_resources, hwnd,
                            get_client_size(hwnd));
}

auto cleanup_submenu(Core::State::AppState& state) -> void {
  cleanup_surface(state.context_menu->submenu_render_resources);
}

auto resize_context_menu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  return resize_surface(state.context_menu->main_render_resources, new_size);
}

auto resize_submenu(Core::State::AppState& state, const SIZE& new_size) -> bool {
  return resize_surface(state.context_menu->submenu_render_resources, new_size);
}

}  // namespace UI::ContextMenu::RenderContext
