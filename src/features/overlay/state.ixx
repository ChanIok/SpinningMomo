module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

export module Features.Overlay.State;

import std;
import Utils.Timer;
import Utils.Graphics.Capture;
import Utils.Graphics.D3D;
import Features.Overlay.Types;

export namespace Features::Overlay::State {

using namespace Features::Overlay::Types;

// 叠加层完整状态
struct OverlayState {
  WindowState window;
  RenderingState rendering;
  CaptureState capture_state;
  InteractionState interaction;
  ThreadState threads;

  std::optional<Utils::Timer::Timer> cleanup_timer;
  std::mutex texture_mutex;
  std::mutex render_target_mutex;
  std::mutex capture_state_mutex;
  std::condition_variable frame_available;
  
  // 添加运行状态标志
  bool running = false;
};

}  // namespace Features::Overlay::State
