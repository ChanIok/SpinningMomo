#pragma once

#include "core/state/app_state.hpp"
#include "core/tasks/state.hpp"

namespace Core::Tasks {

using TaskProgress = Core::Tasks::State::TaskProgress;
using TaskSnapshot = Core::Tasks::State::TaskSnapshot;

auto create_task(Core::State::AppState& state, const std::string& type,
                 const std::optional<std::string>& context = std::nullopt) -> std::string;

auto has_active_task_of_type(Core::State::AppState& state, const std::string& type) -> bool;

auto find_active_task_of_type(Core::State::AppState& state, const std::string& type)
    -> std::optional<TaskSnapshot>;

auto mark_task_running(Core::State::AppState& state, const std::string& task_id) -> bool;

auto update_task_progress(Core::State::AppState& state, const std::string& task_id,
                          const TaskProgress& progress) -> bool;

auto complete_task_success(Core::State::AppState& state, const std::string& task_id) -> bool;

auto complete_task_failed(Core::State::AppState& state, const std::string& task_id,
                          const std::string& error_message) -> bool;

auto list_tasks(Core::State::AppState& state) -> std::vector<TaskSnapshot>;

auto clear_finished_tasks(Core::State::AppState& state) -> std::size_t;

}  // namespace Core::Tasks
