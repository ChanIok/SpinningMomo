module;

#include <asio.hpp>

module Core.RPC.Endpoints.Tasks;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.Tasks;

namespace Core::RPC::Endpoints::Tasks {

struct ClearFinishedTasksResult {
  std::int32_t cleared_count = 0;
};

auto handle_list_tasks(Core::State::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<Core::Tasks::TaskSnapshot>> {
  co_return Core::Tasks::list_tasks(app_state);
}

auto handle_clear_finished_tasks(Core::State::AppState& app_state,
                                 [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<ClearFinishedTasksResult> {
  co_return ClearFinishedTasksResult{
      .cleared_count = static_cast<std::int32_t>(Core::Tasks::clear_finished_tasks(app_state)),
  };
}

auto register_all(Core::State::AppState& app_state) -> void {
  register_method<EmptyParams, std::vector<Core::Tasks::TaskSnapshot>>(
      app_state, app_state.rpc->registry, "task.list", handle_list_tasks,
      "List recent background tasks");

  register_method<EmptyParams, ClearFinishedTasksResult>(
      app_state, app_state.rpc->registry, "task.clearFinished", handle_clear_finished_tasks,
      "Clear finished background tasks");
}

}  // namespace Core::RPC::Endpoints::Tasks
