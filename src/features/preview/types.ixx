module;

#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <atomic>

export module Features.Preview.Types;

import std;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;

export namespace Features::Preview::Types {

// 窗口类名
constexpr wchar_t PREVIEW_WINDOW_CLASS[] = L"SpinningMomoPreviewWindowClass";

// 顶点结构体
struct Vertex {
  float x, y;
  float u, v;
};

// 视口框顶点结构体
struct ViewportVertex {
  struct Position {
    float x, y;
  } pos;
  struct Color {
    float r, g, b, a;
  } color;
};

// 视口状态
struct ViewportState {
  RECT viewport_rect{};
  bool visible = true;
  bool game_window_fully_visible = false;
};

// 交互状态
struct InteractionState {
  bool is_dragging = false;
  bool viewport_dragging = false;
  POINT drag_start{};
};

// DPI相关尺寸
struct DpiDependentSizes {
  static constexpr int BASE_TITLE_HEIGHT = 24;
  static constexpr int BASE_FONT_SIZE = 12;
  static constexpr int BASE_BORDER_WIDTH = 8;

  UINT dpi = 96;
  int title_height = BASE_TITLE_HEIGHT;
  int font_size = BASE_FONT_SIZE;
  int border_width = BASE_BORDER_WIDTH;

  auto update_dpi_scaling(UINT new_dpi) -> void {
    dpi = new_dpi;
    const double scale = static_cast<double>(new_dpi) / 96.0;
    title_height = static_cast<int>(BASE_TITLE_HEIGHT * scale);
    font_size = static_cast<int>(BASE_FONT_SIZE * scale);
    border_width = static_cast<int>(BASE_BORDER_WIDTH * scale);
  }
};

// 窗口尺寸状态
struct WindowSizeState {
  int window_width = 0;
  int window_height = 0;
  float aspect_ratio = 1.0f;
  int ideal_size = 0;
  int min_ideal_size = 0;
  int max_ideal_size = 0;
};

// 渲染相关资源
struct RenderingResources {
  bool initialized = false;
  std::atomic<bool> resources_busy = false;  // 标记渲染资源是否正忙（如尺寸调整等）
  Utils::Graphics::D3D::D3DContext d3d_context;
  Utils::Graphics::D3D::ShaderResources basic_shaders;
  Utils::Graphics::D3D::ShaderResources viewport_shaders;
  Microsoft::WRL::ComPtr<ID3D11Buffer> basic_vertex_buffer;
  Microsoft::WRL::ComPtr<ID3D11Buffer> viewport_vertex_buffer;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> capture_srv;
};

// 捕获会话（业务层封装）
struct CaptureState {
  Utils::Graphics::Capture::CaptureSession session;
  bool active = false;
};

}  // namespace Features::Preview::Types