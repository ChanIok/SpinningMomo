module;

export module Features.Preview.Types;

import std;
import Utils.Throttle;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

namespace Features::Preview::Types {

// 窗口类名
export constexpr wchar_t PREVIEW_WINDOW_CLASS[] = L"SpinningMomoPreviewWindowClass";

// 内部消息常量
export constexpr UINT WM_SCHEDULE_PREVIEW_CLEANUP = WM_USER + 1;
export constexpr UINT WM_CANCEL_PREVIEW_CLEANUP = WM_USER + 2;
export constexpr UINT WM_IMMEDIATE_PREVIEW_CLEANUP = WM_USER + 3;
export constexpr UINT WM_APPLY_CAPTURE_SIZE = WM_USER + 4;

// 顶点结构体
export struct Vertex {
  float x, y;
  float u, v;
};

// 视口框顶点结构体
export struct ViewportVertex {
  struct Position {
    float x, y;
  } pos;
  struct Color {
    float r, g, b, a;
  } color;
};

// 视口状态
export struct ViewportState {
  RECT viewport_rect{};
  RECT visible_game_area{};
  bool visible = true;
  bool game_window_fully_visible = false;
};

// 定时器 ID 常量
export constexpr UINT_PTR TIMER_ID_TASKBAR_REDRAW = 1;
export constexpr UINT_PTR TIMER_ID_PREVIEW_CLEANUP = 2;

// 任务栏重绘延迟时间（毫秒）
export constexpr UINT TASKBAR_REDRAW_DELAY_MS = 200;

// 交互状态
export struct InteractionState {
  bool is_dragging = false;
  bool viewport_dragging = false;
  POINT drag_start{};
  std::unique_ptr<Utils::Throttle::ThrottleState<float, float>> move_throttle;

  // 上次设置的游戏窗口位置（用于跳过重复的 SetWindowPos 调用）
  std::optional<POINT> last_game_window_pos;

  // 任务栏重绘状态
  bool taskbar_redraw_suppressed = false;
};

// DPI相关尺寸
export struct DpiDependentSizes {
  static constexpr int BASE_TITLE_HEIGHT = 24;
  static constexpr int BASE_FONT_SIZE = 12;
  static constexpr int BASE_BORDER_WIDTH = 8;
  static constexpr int BASE_VIEWPORT_LINE_WIDTH = 3;  // 视口框线宽基准值 (3dp)

  UINT dpi = 96;
  int title_height = BASE_TITLE_HEIGHT;
  int font_size = BASE_FONT_SIZE;
  int border_width = BASE_BORDER_WIDTH;
  int viewport_line_width = BASE_VIEWPORT_LINE_WIDTH;

  auto update_dpi_scaling(UINT new_dpi) -> void {
    dpi = new_dpi;
    const double scale = static_cast<double>(new_dpi) / 96.0;
    title_height = static_cast<int>(BASE_TITLE_HEIGHT * scale);
    font_size = static_cast<int>(BASE_FONT_SIZE * scale);
    border_width = static_cast<int>(BASE_BORDER_WIDTH * scale);
    viewport_line_width = static_cast<int>(BASE_VIEWPORT_LINE_WIDTH * scale);
  }
};

// 窗口尺寸状态
export struct WindowSizeState {
  int window_width = 0;
  int window_height = 0;
  float aspect_ratio = 1.0f;
  int ideal_size = 0;
  int min_ideal_size = 0;
  int max_ideal_size = 0;
};

// 渲染相关资源
export struct RenderingResources {
  std::atomic<bool> initialized = false;
  std::atomic<bool> resources_busy = false;  // 标记渲染资源是否正忙（如尺寸调整等）
  Utils::Graphics::D3D::D3DContext d3d_context;
  Utils::Graphics::D3D::ShaderResources basic_shaders;
  Utils::Graphics::D3D::ShaderResources viewport_shaders;
  wil::com_ptr<ID3D11Buffer> basic_vertex_buffer;
  wil::com_ptr<ID3D11Buffer> viewport_vertex_buffer;
  wil::com_ptr<ID3D11ShaderResourceView> capture_srv;
};

// 捕获会话（业务层封装）
export struct CaptureState {
  Utils::Graphics::Capture::CaptureSession session;
  int last_frame_width = 0;
  int last_frame_height = 0;
};

}  // namespace Features::Preview::Types
