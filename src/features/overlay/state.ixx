module;

export module Features.Overlay.State;

import std;
import Utils.Timer;
import Features.Overlay.Types;

export namespace Features::Overlay::State {

// 叠加层完整状态
struct OverlayState {
  Types::WindowState window;
  Types::RenderingState rendering;
  Types::CaptureState capture_state;
  Types::InteractionState interaction;
  Types::ThreadState threads;

  std::optional<Utils::Timer::Timer> cleanup_timer;
  std::mutex texture_mutex;
  std::mutex render_target_mutex;
  std::mutex capture_state_mutex;
  std::condition_variable frame_available;

  // 状态标志
  bool enabled = false;  // 用户是否启用叠加层模式
  bool running = false;  // 叠加层是否实际在运行
};

}  // namespace Features::Overlay::State
