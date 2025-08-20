module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Features.FileDialog.Rpc;

import std;
import Core.State;
import Core.RpcHandlers;
import Core.RpcHandlers.Types;
import Core.RpcHandlers.State;
import Features.FileDialog;
import Features.FileDialog.Types;
import Utils.Logger;

namespace Features::FileDialog::Rpc {

auto handle_select_file([[maybe_unused]] Core::State::AppState& app_state,
                        const Features::FileDialog::Types::FileSelectorParams& params)
    -> asio::awaitable<
        Core::RpcHandlers::RpcResult<Features::FileDialog::Types::FileSelectorResult>> {
  auto result = Features::FileDialog::select_file(app_state, params);
  if (result) {
    co_return result.value();
  }
  co_return std::unexpected(Core::RpcHandlers::RpcError{
      .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
      .message = "Service error: " + result.error(),
  });
}

auto handle_select_folder([[maybe_unused]] Core::State::AppState& app_state,
                          const Features::FileDialog::Types::FolderSelectorParams& params)
    -> asio::awaitable<
        Core::RpcHandlers::RpcResult<Features::FileDialog::Types::FolderSelectorResult>> {
  auto result = Features::FileDialog::select_folder(app_state, params);
  if (result) {
    co_return result.value();
  }
  co_return std::unexpected(Core::RpcHandlers::RpcError{
      .code = static_cast<int>(Core::RpcHandlers::ErrorCode::ServerError),
      .message = "Service error: " + result.error(),
  });
}

auto register_handlers(Core::State::AppState& app_state) -> void {
  Core::RpcHandlers::register_method<Features::FileDialog::Types::FileSelectorParams,
                                     Features::FileDialog::Types::FileSelectorResult>(
      app_state, app_state.rpc_handlers->registry, "file_dialog.select_file", handle_select_file,
      "Open a file picker and return selected file paths");

  Core::RpcHandlers::register_method<Features::FileDialog::Types::FolderSelectorParams,
                                     Features::FileDialog::Types::FolderSelectorResult>(
      app_state, app_state.rpc_handlers->registry, "file_dialog.select_folder",
      handle_select_folder, "Open a folder picker and return selected path");

  Logger().info("Registered file dialog RPC methods: select_file, select_folder");
}

}  // namespace Features::FileDialog::Rpc