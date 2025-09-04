module;

export module Core.Events.State;

import std;

namespace Core::Events::State {

export struct EventsState {
  std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>> handlers;
  std::queue<std::pair<std::type_index, std::any>> event_queue;
  std::mutex queue_mutex;
};

}  // namespace Core::Events::State