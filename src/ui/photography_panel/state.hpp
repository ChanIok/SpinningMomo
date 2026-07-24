#pragma once

#include <d2d1_3.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <wil/com.h>
#include <windows.h>

#include "ui/floating_window/types.hpp"

namespace UI::PhotographyPanel::State {

constexpr wchar_t kWindowClassName[] = L"SpinningMomoPhotographyPanelClass";
constexpr int kPanelWidth = 340;
constexpr int kWindowRightMargin = 24;
constexpr int kWindowTopMargin = 48;
constexpr int kSliderTrackHalfHeight = 8;
constexpr float kSliderKnobRadius = 7.0f;
constexpr int kSliderTrackHitHalfHeight = 12;
constexpr float kSliderKnobHoverStrokeWidth = 1.0f;

struct PanelLayoutMetrics {
  SIZE window_size{};
  RECT title_rect{};
  RECT title_text_rect{};
  RECT label_rect{};
  RECT slider_row_rect{};
  RECT slider_rect{};
};

struct RenderResources {
  wil::com_ptr<IDXGISwapChain1> swap_chain;
  wil::com_ptr<IDCompositionTarget> composition_target;
  wil::com_ptr<IDCompositionVisual> composition_visual;

  wil::com_ptr<ID2D1DeviceContext6> device_context;
  wil::com_ptr<ID2D1Bitmap1> target_bitmap;

  wil::com_ptr<IDWriteTextFormat> text_format;
  wil::com_ptr<ID2D1SolidColorBrush> background_brush;
  wil::com_ptr<ID2D1SolidColorBrush> title_brush;
  wil::com_ptr<ID2D1SolidColorBrush> text_brush;
  wil::com_ptr<ID2D1SolidColorBrush> track_brush;
  wil::com_ptr<ID2D1SolidColorBrush> knob_brush;

  SIZE surface_size = {0, 0};
  bool is_ready = false;
  bool is_rendering = false;
};

struct PhotographyPanelState {
  HWND hwnd = nullptr;
  bool is_visible = false;
  bool dragging_long_exposure = false;
  bool knob_hovered = false;
  PanelLayoutMetrics layout;
  RenderResources render_resources;
};

}  // namespace UI::PhotographyPanel::State
