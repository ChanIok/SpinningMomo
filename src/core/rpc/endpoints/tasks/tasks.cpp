#include "core/rpc/endpoints/tasks/tasks.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "core/tasks/tasks.hpp"

namespace core::rpc::endpoints::tasks {

struct ClearFinishedTasksResult {
  std::int32_t cleared_count = 0;
};

auto handle_list_tasks(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<core::tasks::TaskSnapshot>> {
  co_return core::tasks::list_tasks(app_state);
}

auto handle_clear_finished_tasks(core::AppState& app_state,
                                 [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<ClearFinishedTasksResult> {
  co_return ClearFinishedTasksResult{
      .cleared_count = static_cast<std::int32_t>(core::tasks::clear_finished_tasks(app_state)),
  };
}

auto register_all(core::AppState& app_state) -> void {
  // 远端只允许查看和清理后台任务结果，不直接授予宿主机配置能力。
  register_method<EmptyParams, std::vector<core::tasks::TaskSnapshot>>(
      app_state, app_state.rpc->registry, "task.list", handle_list_tasks,
      "List recent background tasks", AccessLevel::lan);

  register_method<EmptyParams, ClearFinishedTasksResult>(
      app_state, app_state.rpc->registry, "task.clearFinished", handle_clear_finished_tasks,
      "Clear finished background tasks", AccessLevel::lan);
}

}  // namespace core::rpc::endpoints::tasks
