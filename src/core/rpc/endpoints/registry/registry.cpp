#include "core/rpc/endpoints/registry/registry.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/commands/registry.hpp"
#include "core/commands/types.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"

namespace core::rpc::endpoints::registry {

// 返回命令注册表的可传输元数据，不复制内部 action 和状态读取器
auto handle_get_all_commands(core::AppState& app_state,
                             const core::commands::GetAllCommandsParams& params)
    -> asio::awaitable<core::rpc::RpcResult<core::commands::GetAllCommandsResult>> {
  try {
    // 命令模块已经生成隔离行为字段的元数据快照，RPC 只负责转交
    co_return core::commands::GetAllCommandsResult{
        .commands = core::commands::get_all_commands(app_state),
    };
  } catch (const std::exception& e) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Failed to get commands: " + std::string(e.what())});
  }
}

// 校验命令 ID 后调用注册表，并返回统一的 RPC 结果
auto handle_invoke_command(core::AppState& app_state,
                           const core::commands::InvokeCommandParams& params)
    -> asio::awaitable<core::rpc::RpcResult<core::commands::InvokeCommandResult>> {
  try {
    if (params.id.empty()) {
      co_return std::unexpected(
          core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::InvalidParams),
                              .message = "Command id cannot be empty"});
    }

    const auto success = core::commands::invoke_command(app_state, params.id);

    core::commands::InvokeCommandResult result{
        .success = success,
        .message =
            success ? "Command invoked successfully" : "Command not found or failed: " + params.id,
    };

    co_return result;
  } catch (const std::exception& e) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Failed to invoke command: " + std::string(e.what())});
  }
}

// 注册命令查询与调用相关的 RPC 方法，并限制命令调用只来自本机。
auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<core::commands::GetAllCommandsParams,
                             core::commands::GetAllCommandsResult>(
      app_state, app_state.rpc->registry, "commands.getAll", handle_get_all_commands,
      "Get all available command descriptors", core::rpc::AccessLevel::lan);

  core::rpc::register_method<core::commands::InvokeCommandParams,
                             core::commands::InvokeCommandResult>(
      app_state, app_state.rpc->registry, "commands.invoke", handle_invoke_command,
      "Invoke a command by id");
}

}  // namespace core::rpc::endpoints::registry
