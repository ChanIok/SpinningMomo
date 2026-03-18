module;

export module Core.Tasks;

import std;
import Core.State;
import Core.Tasks.State;

namespace Core::Tasks {

export using TaskProgress = Core::Tasks::State::TaskProgress;
export using TaskSnapshot = Core::Tasks::State::TaskSnapshot;

export auto create_task(Core::State::AppState& state, const std::string& type,
                        const std::optional<std::string>& context = std::nullopt) -> std::string;

export auto has_active_task_of_type(Core::State::AppState& state, const std::string& type) -> bool;

export auto mark_task_running(Core::State::AppState& state, const std::string& task_id) -> bool;

export auto update_task_progress(Core::State::AppState& state, const std::string& task_id,
                                 const TaskProgress& progress) -> bool;

export auto complete_task_success(Core::State::AppState& state, const std::string& task_id) -> bool;

export auto complete_task_failed(Core::State::AppState& state, const std::string& task_id,
                                 const std::string& error_message) -> bool;

export auto list_tasks(Core::State::AppState& state) -> std::vector<TaskSnapshot>;

export auto clear_finished_tasks(Core::State::AppState& state) -> std::size_t;

}  // namespace Core::Tasks
