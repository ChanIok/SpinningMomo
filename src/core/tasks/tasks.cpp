module;

module Core.Tasks;

import std;
import Core.State;
import Core.Tasks.State;
import Core.RPC.NotificationHub;
import Utils.Logger;
import <rfl/json.hpp>;

namespace Core::Tasks {

auto now_millis() -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

auto is_task_active(const std::string& status) -> bool {
  return status == "queued" || status == "running";
}

auto emit_task_updated(Core::State::AppState& state, const TaskSnapshot& snapshot) -> void {
  auto params_json = rfl::json::write<rfl::SnakeCaseToCamelCase>(snapshot);
  Core::RPC::NotificationHub::send_notification(state, "task.updated", params_json);
}

auto prune_history_unlocked(State::TaskState& task_state) -> void {
  while (task_state.order.size() > task_state.history_limit) {
    const auto& oldest_id = task_state.order.front();
    auto it = task_state.tasks.find(oldest_id);
    if (it == task_state.tasks.end()) {
      task_state.order.pop_front();
      continue;
    }

    // 仍在运行中的任务不裁剪，避免列表状态突然丢失。
    if (is_task_active(it->second.status)) {
      break;
    }

    task_state.tasks.erase(it);
    task_state.order.pop_front();
  }
}

auto create_task(Core::State::AppState& state, const std::string& type,
                 const std::optional<std::string>& context) -> std::string {
  if (!state.tasks) {
    Logger().error("TaskState is not initialized");
    return "";
  }

  TaskSnapshot snapshot;
  {
    std::lock_guard<std::mutex> lock(state.tasks->mutex);

    auto task_id = std::format("task_{}_{}", now_millis(), state.tasks->next_task_id++);
    snapshot.task_id = task_id;
    snapshot.type = type;
    snapshot.status = "queued";
    snapshot.created_at = now_millis();
    snapshot.context = context;

    state.tasks->tasks[task_id] = snapshot;
    state.tasks->order.push_back(task_id);
    prune_history_unlocked(*state.tasks);
  }

  emit_task_updated(state, snapshot);
  return snapshot.task_id;
}

auto has_active_task_of_type(Core::State::AppState& state, const std::string& type) -> bool {
  return find_active_task_of_type(state, type).has_value();
}

auto find_active_task_of_type(Core::State::AppState& state, const std::string& type)
    -> std::optional<TaskSnapshot> {
  if (!state.tasks) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(state.tasks->mutex);
  // 反向遍历以优先返回最新创建的活跃任务
  for (auto it = state.tasks->order.rbegin(); it != state.tasks->order.rend(); ++it) {
    auto task_it = state.tasks->tasks.find(*it);
    if (task_it == state.tasks->tasks.end()) {
      continue;
    }

    const auto& snapshot = task_it->second;
    if (snapshot.type == type && is_task_active(snapshot.status)) {
      return snapshot;
    }
  }

  return std::nullopt;
}

auto mark_task_running(Core::State::AppState& state, const std::string& task_id) -> bool {
  if (!state.tasks) {
    return false;
  }

  std::optional<TaskSnapshot> snapshot;
  {
    std::lock_guard<std::mutex> lock(state.tasks->mutex);
    auto it = state.tasks->tasks.find(task_id);
    if (it == state.tasks->tasks.end()) {
      return false;
    }

    auto& task = it->second;
    task.status = "running";
    task.started_at = now_millis();
    task.finished_at.reset();
    task.error_message.reset();
    snapshot = task;
  }

  emit_task_updated(state, snapshot.value());
  return true;
}

auto update_task_progress(Core::State::AppState& state, const std::string& task_id,
                          const TaskProgress& progress) -> bool {
  if (!state.tasks) {
    return false;
  }

  std::optional<TaskSnapshot> snapshot;
  {
    std::lock_guard<std::mutex> lock(state.tasks->mutex);
    auto it = state.tasks->tasks.find(task_id);
    if (it == state.tasks->tasks.end()) {
      return false;
    }

    auto& task = it->second;
    if (task.status == "succeeded" || task.status == "failed" || task.status == "cancelled") {
      return false;
    }

    if (task.status == "queued") {
      task.status = "running";
      task.started_at = now_millis();
    }

    task.progress = progress;
    snapshot = task;
  }

  emit_task_updated(state, snapshot.value());
  return true;
}

auto complete_task_success(Core::State::AppState& state, const std::string& task_id) -> bool {
  if (!state.tasks) {
    return false;
  }

  std::optional<TaskSnapshot> snapshot;
  {
    std::lock_guard<std::mutex> lock(state.tasks->mutex);
    auto it = state.tasks->tasks.find(task_id);
    if (it == state.tasks->tasks.end()) {
      return false;
    }

    auto& task = it->second;
    task.status = "succeeded";
    if (!task.started_at.has_value()) {
      task.started_at = task.created_at;
    }
    task.finished_at = now_millis();
    task.error_message.reset();
    if (task.progress.has_value() && !task.progress->percent.has_value()) {
      task.progress->percent = 100.0;
    }
    snapshot = task;
    prune_history_unlocked(*state.tasks);
  }

  emit_task_updated(state, snapshot.value());
  return true;
}

auto complete_task_failed(Core::State::AppState& state, const std::string& task_id,
                          const std::string& error_message) -> bool {
  if (!state.tasks) {
    return false;
  }

  std::optional<TaskSnapshot> snapshot;
  {
    std::lock_guard<std::mutex> lock(state.tasks->mutex);
    auto it = state.tasks->tasks.find(task_id);
    if (it == state.tasks->tasks.end()) {
      return false;
    }

    auto& task = it->second;
    task.status = "failed";
    if (!task.started_at.has_value()) {
      task.started_at = task.created_at;
    }
    task.finished_at = now_millis();
    task.error_message = error_message;
    snapshot = task;
    prune_history_unlocked(*state.tasks);
  }

  emit_task_updated(state, snapshot.value());
  return true;
}

auto list_tasks(Core::State::AppState& state) -> std::vector<TaskSnapshot> {
  if (!state.tasks) {
    return {};
  }

  std::vector<TaskSnapshot> result;
  std::lock_guard<std::mutex> lock(state.tasks->mutex);
  result.reserve(state.tasks->order.size());

  for (auto it = state.tasks->order.rbegin(); it != state.tasks->order.rend(); ++it) {
    if (auto task_it = state.tasks->tasks.find(*it); task_it != state.tasks->tasks.end()) {
      result.push_back(task_it->second);
    }
  }

  return result;
}

auto clear_finished_tasks(Core::State::AppState& state) -> std::size_t {
  if (!state.tasks) {
    return 0;
  }

  std::size_t cleared_count = 0;
  std::lock_guard<std::mutex> lock(state.tasks->mutex);

  std::deque<std::string> active_order;
  for (const auto& task_id : state.tasks->order) {
    auto it = state.tasks->tasks.find(task_id);
    if (it == state.tasks->tasks.end()) {
      continue;
    }

    if (is_task_active(it->second.status)) {
      active_order.push_back(task_id);
      continue;
    }

    state.tasks->tasks.erase(it);
    cleared_count++;
  }

  state.tasks->order = std::move(active_order);
  return cleared_count;
}

}  // namespace Core::Tasks
