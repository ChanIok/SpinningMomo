module;

export module Features.WindowControl.State;

import std;
import Vendor.Windows;

namespace Features::WindowControl::State {

export struct WindowControlState {
  std::jthread center_lock_monitor_thread;
  // 退出时用于立即唤醒监控线程，避免 join() 额外等待一个轮询周期。
  std::mutex center_lock_monitor_mutex;
  std::condition_variable_any center_lock_monitor_cv;
  // 仅在当前 clip 区域仍然是本模块上次写入的小矩形时才负责释放，
  // 避免误清掉游戏后续自己重新设置的 ClipCursor 状态。
  bool center_lock_owned{false};
  Vendor::Windows::RECT last_center_lock_rect{};
};

export constexpr auto kCenterLockPollInterval = std::chrono::milliseconds{50};
export constexpr int kCenterLockSize = 1;
export constexpr int kClipTolerance = 10;

}  // namespace Features::WindowControl::State
