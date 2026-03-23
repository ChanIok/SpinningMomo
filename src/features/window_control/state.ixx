module;

export module Features.WindowControl.State;

import std;
import Vendor.Windows;

namespace Features::WindowControl::State {

export struct WindowControlState {
  std::jthread center_lock_monitor_thread;
  std::mutex center_lock_monitor_mutex;
  std::condition_variable_any center_lock_monitor_cv;
  bool center_lock_owned{false};
  Vendor::Windows::RECT last_center_lock_rect{};
};

export constexpr auto kCenterLockPollInterval = std::chrono::milliseconds{50};
export constexpr int kCenterLockSize = 1;
export constexpr int kClipTolerance = 10;

}  // namespace Features::WindowControl::State
