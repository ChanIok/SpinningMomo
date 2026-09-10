#include "ui/notification_window/render_context.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/d2d1_3.hpp"
#include "vendor/windows/d3d11.hpp"
#include "vendor/windows/dcomp.hpp"
#include "vendor/windows/dwrite_3.hpp"
#include "vendor/windows/dxgi1_2.hpp"

#include "core/state/app_state.hpp"
#include "ui/composition_animation/animation.hpp"
#include "ui/notification_window/painter.hpp"
#include "ui/notification_window/state.hpp"
#include "ui/notification_window/types.hpp"
#include "ui/shared_render_resources/shared_render_resources.hpp"
#include "ui/shared_render_resources/state.hpp"
#include "utils/logger/logger.hpp"

namespace ui::notification_window::render_context {

constexpr DXGI_FORMAT kSurfaceFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

auto shared_resources(core::AppState& state)
    -> ui::shared_render_resources::SharedRenderResourcesState& {
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

auto release_text_formats(notification_window::RenderResources& render_resources) -> void {
  render_resources.title_text_format.reset();
  render_resources.message_text_format.reset();
  render_resources.button_text_format.reset();
}

auto release_brushes(notification_window::RenderResources& render_resources) -> void {
  render_resources.fill_brush.reset();
  render_resources.stroke_brush.reset();
  render_resources.text_brush.reset();
}

auto create_device_context(ID2D1Device* shared_device,
                           notification_window::RenderResources& render_resources) -> bool {
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

auto create_composition_tree(ID3D11Device* shared_d3d_device,
                             notification_window::RenderResources& render_resources, HWND hwnd)
    -> bool {
  if (!shared_d3d_device) {
    return false;
  }

  wil::com_ptr<IDXGIDevice> dxgi_device;
  if (FAILED(shared_d3d_device->QueryInterface(IID_PPV_ARGS(dxgi_device.put()))) || !dxgi_device) {
    return false;
  }

  auto& composition_device = render_resources.composition_device;
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

  return SUCCEEDED(render_resources.composition_target->SetRoot(
             render_resources.composition_visual.get())) &&
         SUCCEEDED(composition_device->Commit());
}

auto create_brushes(notification_window::RenderResources& render_resources) -> bool {
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

auto ensure_text_formats(core::AppState& state,
                         notification_window::RenderResources& render_resources, int dpi) -> bool {
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
      write_factory, scale_for_dpi(notification_window::BASE_TITLE_FONT_SIZE),
      DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
  render_resources.message_text_format =
      create_text_format(write_factory, scale_for_dpi(notification_window::BASE_FONT_SIZE),
                         DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
                         DWRITE_WORD_WRAPPING_CHARACTER);
  render_resources.button_text_format =
      create_text_format(write_factory, scale_for_dpi(notification_window::BASE_FONT_SIZE),
                         DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                         DWRITE_WORD_WRAPPING_NO_WRAP);

  render_resources.dpi = dpi;
  return render_resources.title_text_format && render_resources.message_text_format &&
         render_resources.button_text_format;
}

auto reset_render_context(notification_window::RenderResources& render_resources) -> void {
  release_text_formats(render_resources);
  release_brushes(render_resources);
  render_resources.device_context.reset();
  render_resources.composition_visual.reset();
  render_resources.composition_target.reset();
  render_resources.composition_device.reset();
  render_resources.is_ready = false;
  render_resources.is_rendering = false;
  render_resources.dpi = 96;
}

auto ensure_render_context(core::AppState& state) -> bool {
  auto& window_state = *state.notification_window;
  auto& render_resources = window_state.render_resources;
  if (!window_state.host_hwnd) {
    return false;
  }
  if (!ui::shared_render_resources::ensure_initialized(state)) {
    return false;
  }

  const SIZE size = get_client_size(window_state.host_hwnd);
  if (size.cx <= 0 || size.cy <= 0) {
    return false;
  }

  auto& shared = shared_resources(state);
  if (!render_resources.is_ready) {
    cleanup_render_context(state);
    if (!create_device_context(shared.d2d_device.get(), render_resources) ||
        !create_composition_tree(shared.d3d_device.get(), render_resources,
                                 window_state.host_hwnd) ||
        !create_brushes(render_resources)) {
      cleanup_render_context(state);
      return false;
    }
  }

  if (!ensure_text_formats(state, render_resources,
                           static_cast<int>(GetDpiForWindow(window_state.host_hwnd)))) {
    cleanup_render_context(state);
    return false;
  }

  render_resources.is_ready = true;
  return true;
}

auto cleanup_render_context(core::AppState& state) -> void {
  for (auto& notification : state.notification_window->active_notifications) {
    notification.visual.reset();
    notification.opacity_effect.reset();
    notification.surface.reset();
    notification.content_dirty = true;
    notification.motion.dirty = true;
  }
  reset_render_context(state.notification_window->render_resources);
}

// 确保指定卡片的 DComp 资源就绪：创建 DComp 表面 → 创建 Visual 与效果组 → 挂载到根 Visual →
// 绑定表面
auto ensure_card(core::AppState& state, Notification& notification) -> bool {
  // 若已有有效表面和 Visual，无需重建
  if (notification.visual && notification.surface) {
    return true;
  }
  auto& resources = state.notification_window->render_resources;
  // 获取阴影留白尺寸
  const int padding = painter::get_surface_padding(resources.dpi);
  wil::com_ptr<IDCompositionSurface> surface;
  // 创建包含阴影留白的卡片独立 DComp 表面
  HRESULT hr = resources.composition_device->CreateSurface(
      notification.width + padding * 2, notification.height + padding * 2, kSurfaceFormat,
      DXGI_ALPHA_MODE_PREMULTIPLIED, surface.put());
  if (SUCCEEDED(hr) && !notification.visual) {
    // 创建卡片子 Visual
    hr = resources.composition_device->CreateVisual(notification.visual.put());
    // 创建并关联透明度效果组
    if (SUCCEEDED(hr)) {
      hr = resources.composition_device->CreateEffectGroup(notification.opacity_effect.put());
    }
    if (SUCCEEDED(hr)) {
      hr = notification.visual->SetEffect(notification.opacity_effect.get());
    }
    // 设置双线性位图插值模式以保证动画平滑
    if (SUCCEEDED(hr)) {
      hr = notification.visual->SetBitmapInterpolationMode(
          DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
    }
    // 将卡片 Visual 挂载到根 Visual 树
    if (SUCCEEDED(hr)) {
      hr = resources.composition_visual->AddVisual(notification.visual.get(), TRUE, nullptr);
    }
  }
  // 将表面设置为卡片 Visual 的显示内容
  if (SUCCEEDED(hr)) {
    hr = notification.visual->SetContent(surface.get());
  }
  if (FAILED(hr)) {
    Logger().error("Failed to create notification card: 0x{:X}", hr);
    return false;
  }
  // 记录表面与留白并标记内容与运动待提交
  notification.surface = std::move(surface);
  notification.surface_padding = padding;
  notification.content_dirty = true;
  notification.motion.dirty = true;
  return true;
}

// 将卡片的位移与透明度运动状态应用到 DComp Visual：评估过渡属性 → 生成硬件动画 → 绑定属性
auto apply_motion(core::AppState& state, Notification& notification) -> bool {
  // 运动无变更则直接返回
  if (!notification.motion.dirty) {
    return true;
  }
  auto* device = state.notification_window->render_resources.composition_device.get();
  // 辅助闭包：为数值属性生成动画或直接设值
  auto set_property = [&](const ui::composition_animation::Transition& transition, float adjustment,
                          auto setter) -> bool {
    auto adjusted = transition;
    adjusted.from += adjustment;
    adjusted.to += adjustment;
    // 无过渡时长或起始与目标相同时直接赋终值
    if (adjusted.seconds <= 0.0 || adjusted.from == adjusted.to) {
      return SUCCEEDED(setter(adjusted.to));
    }
    // 生成 DComp 硬件动画并绑定到属性
    auto animation = ui::composition_animation::create(device, adjusted);
    return animation && SUCCEEDED(setter(animation->get()));
  };
  auto* visual = notification.visual.get();
  // 运动使用卡片内容坐标，visual 原点需向外偏移阴影留白
  const float padding = static_cast<float>(notification.surface_padding);
  // 分别应用 X 轴位移、Y 轴位移和透明度
  const bool success = set_property(notification.motion.offset_x, -padding,
                                    [visual](auto value) { return visual->SetOffsetX(value); }) &&
                       set_property(notification.motion.offset_y, -padding,
                                    [visual](auto value) { return visual->SetOffsetY(value); }) &&
                       set_property(notification.motion.opacity, 0.0f, [&notification](auto value) {
                         return notification.opacity_effect->SetOpacity(value);
                       });
  if (!success) {
    Logger().error("Failed to update notification motion");
  }
  return success;
}

// 从 Visual 树移除卡片并释放合成资源：移除子节点 → 标记合成脏 → 释放 Visual 与表面
auto remove_card(core::AppState& state, Notification& notification) -> void {
  auto& resources = state.notification_window->render_resources;
  // 从根 Visual 树中解绑当前卡片的子节点
  if (notification.visual && resources.composition_visual) {
    const HRESULT hr = resources.composition_visual->RemoveVisual(notification.visual.get());
    if (FAILED(hr)) {
      Logger().error("Failed to remove notification visual: 0x{:X}", hr);
    } else {
      state.notification_window->composition_dirty = true;
    }
  }
  // 释放 COM 指针资源
  notification.visual.reset();
  notification.opacity_effect.reset();
  notification.surface.reset();
}

// 提交当前 DComp 设备上的所有事务变更到系统合成器
auto commit(core::AppState& state) -> bool {
  auto& resources = state.notification_window->render_resources;
  if (!resources.composition_device) {
    return false;
  }
  // 提交合成事务
  const HRESULT hr = resources.composition_device->Commit();
  if (FAILED(hr)) {
    Logger().error("Failed to commit notification composition: 0x{:X}", hr);
  }
  return SUCCEEDED(hr);
}

}  // namespace ui::notification_window::render_context
