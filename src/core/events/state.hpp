#pragma once

#include <windows.h>

namespace Core::Events::State {

struct EventsState {
  std::unordered_map<std::type_index,
                     std::vector<std::move_only_function<void(const std::any&) const>>>
      handlers;
  std::queue<std::pair<std::type_index, std::any>> event_queue;
  std::mutex queue_mutex;

  // Window handle for UI thread wake-up notifications
  HWND notify_hwnd = nullptr;
};

}  // namespace Core::Events::State
