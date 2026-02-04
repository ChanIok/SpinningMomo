module;

export module Features.Overlay.Types;

import std;
import Utils.Timer;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import <d3d11.h>;
import <wil/com.h>;
import <windows.h>;

export namespace Features::Overlay::Types {

// 消息常量
constexpr UINT WM_GAME_WINDOW_FOREGROUND = WM_USER + 1;
constexpr UINT WM_MOUSE_EVENT = WM_USER + 2;
constexpr UINT WM_WINDOW_EVENT = WM_USER + 3;

// 顶点结构体
struct Vertex {
  float x, y;
  float u, v;
};

// 窗口状态
struct WindowState {
  HWND overlay_hwnd = nullptr;
  HWND target_window = nullptr;
  HWND timer_window = nullptr;

  int screen_width = 0;
  int screen_height = 0;
  int window_width = 0;
  int window_height = 0;
  int cached_game_width = 0;
  int cached_game_height = 0;
  RECT game_window_rect{};

  bool use_letterbox_mode = false;

  bool overlay_window_shown = false;
};

// 渲染状态
struct RenderingState {
  Utils::Graphics::D3D::D3DContext d3d_context;
  Utils::Graphics::D3D::ShaderResources shader_resources;
  wil::com_ptr<ID3D11Texture2D> frame_texture;
  wil::com_ptr<ID3D11ShaderResourceView> capture_srv;
  HANDLE frame_latency_object = nullptr;
  std::atomic<bool> resources_busy = false;  // 标记渲染资源是否正忙（如尺寸调整等）

  bool d3d_initialized = false;
  bool create_new_srv = true;
};

// 捕获状态
struct CaptureState {
  Utils::Graphics::Capture::CaptureSession session;
  int last_frame_width = 0;
  int last_frame_height = 0;
};

// 交互状态
struct InteractionState {
  HHOOK mouse_hook = nullptr;
  HWINEVENTHOOK event_hook = nullptr;
  POINT current_mouse_pos{};
  POINT last_mouse_pos{};
  DWORD game_process_id = 0;
  bool is_game_focused = false;            // 前台窗口是否是游戏/overlay
  bool taskbar_redraw_suppressed = false;  // 任务栏重绘是否已禁用
};

// 线程状态
struct ThreadState {
  std::jthread hook_thread;
  std::jthread window_manager_thread;

  DWORD hook_thread_id = 0;
  DWORD window_manager_thread_id = 0;
};

}  // namespace Features::Overlay::Types