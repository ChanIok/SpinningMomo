module;

module Core.RPC.Endpoints.Registry;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.Commands;
import Core.Commands.State;
import <asio.hpp>;

namespace Core::RPC::Endpoints::Registry {

auto handle_get_all_commands(Core::State::AppState& app_state,
                             const Core::Commands::GetAllCommandsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Core::Commands::GetAllCommandsResult>> {
  try {
    if (!app_state.commands) {
      co_return std::unexpected(
          Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                              .message = "Command registry not initialized"});
    }

    // 获取所有命令描述符
    auto all_commands = Core::Commands::get_all_commands(app_state.commands->registry);

    // 转换为 RPC 传输格式
    Core::Commands::GetAllCommandsResult result;
    result.commands.reserve(all_commands.size());

    for (const auto& command : all_commands) {
      Core::Commands::CommandDescriptorData data{
          .id = command.id,
          .i18n_key = command.i18n_key,
          .is_toggle = command.is_toggle,
      };
      result.commands.push_back(std::move(data));
    }

    co_return result;
  } catch (const std::exception& e) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to get commands: " + std::string(e.what())});
  }
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<Core::Commands::GetAllCommandsParams,
                             Core::Commands::GetAllCommandsResult>(
      app_state, app_state.rpc->registry, "commands.getAll", handle_get_all_commands,
      "Get all available command descriptors");
}

}  // namespace Core::RPC::Endpoints::Registry
