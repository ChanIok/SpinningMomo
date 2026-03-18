module;

module Core.RPC.Endpoints.Clipboard;

import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Utils.System;
import <asio.hpp>;

namespace Core::RPC::Endpoints::Clipboard {

auto handle_read_text([[maybe_unused]] Core::State::AppState& app_state,
                      [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::optional<std::string>> {
  auto result = Utils::System::read_clipboard_text();
  if (!result) {
    co_return std::unexpected(
        RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                 .message = "Failed to read clipboard text: " + result.error()});
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  register_method<EmptyParams, std::optional<std::string>>(
      app_state, app_state.rpc->registry, "clipboard.readText", handle_read_text,
      "Read plain text from the system clipboard");
}

}  // namespace Core::RPC::Endpoints::Clipboard
