module;

export module Core.Tasks.State;

import std;

namespace Core::Tasks::State {

export struct TaskProgress {
  std::string stage;
  std::int64_t current = 0;
  std::int64_t total = 0;
  std::optional<double> percent;
  std::optional<std::string> message;
};

export struct TaskSnapshot {
  std::string task_id;
  std::string type;
  std::string status = "queued";
  std::int64_t created_at = 0;
  std::optional<std::int64_t> started_at;
  std::optional<std::int64_t> finished_at;
  std::optional<TaskProgress> progress;
  std::optional<std::string> error_message;
  std::optional<std::string> context;
};

export struct TaskState {
  std::unordered_map<std::string, TaskSnapshot> tasks;
  std::deque<std::string> order;  // 旧 -> 新
  std::mutex mutex;
  std::uint64_t next_task_id = 1;
  size_t history_limit = 30;
};

}  // namespace Core::Tasks::State
