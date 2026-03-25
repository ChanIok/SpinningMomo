module;

export module Features.Preview.State;

import std;
import Features.Preview.Types;
import <windows.h>;

export namespace Features::Preview::State {

// 预览窗口完整状态
struct PreviewState {
  // 窗口句柄
  HWND hwnd = nullptr;
  HWND target_window = nullptr;

  // 窗口状态
  bool is_first_show = true;

  // 尺寸相关
  Features::Preview::Types::WindowSizeState size;
  Features::Preview::Types::DpiDependentSizes dpi_sizes;

  // 交互状态
  Features::Preview::Types::InteractionState interaction;
  Features::Preview::Types::ViewportState viewport;

  // 渲染状态
  std::atomic<bool> running = false;
  std::atomic<bool> create_new_srv = true;
  Features::Preview::Types::RenderingResources rendering_resources;

  // 捕获状态
  Features::Preview::Types::CaptureState capture_state;

  // 游戏窗口缓存信息
  RECT game_window_rect{};
};

}  // namespace Features::Preview::State
