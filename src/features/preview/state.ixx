module;

#include <windows.h>

export module Features.Preview.State;

import std;
import Utils.Timer;
import Features.Preview.Types;

export namespace Features::Preview::State {

// 预览窗口完整状态
struct PreviewState {
  // 窗口句柄
  HWND hwnd = nullptr;
  HWND target_window = nullptr;

  // 窗口状态
  bool is_visible = false;
  bool is_first_show = true;

  // 尺寸相关
  Features::Preview::Types::WindowSizeState size;
  Features::Preview::Types::DpiDependentSizes dpi_sizes;

  // 交互状态
  Features::Preview::Types::InteractionState interaction;
  Features::Preview::Types::ViewportState viewport;

  // 渲染状态
  bool d3d_initialized = false;
  bool running = false;
  bool create_new_srv = true;
  Features::Preview::Types::RenderingResources rendering_resources;

  // 捕获状态
  Features::Preview::Types::CaptureState capture_state;

  // 定时器
  std::optional<Utils::Timer::Timer> cleanup_timer;

  // 游戏窗口缓存信息
  RECT game_window_rect{};
};

}  // namespace Features::Preview::State