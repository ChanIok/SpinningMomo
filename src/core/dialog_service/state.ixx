module;

export module Core.DialogService.State;

import std;

namespace Core::DialogService::State {

export struct DialogServiceState {
  std::jthread worker_thread;

  std::queue<std::function<void()>> task_queue;
  std::mutex queue_mutex;
  std::condition_variable condition;

  std::atomic<bool> is_running{false};
  std::atomic<bool> shutdown_requested{false};
};

}  // namespace Core::DialogService::State
