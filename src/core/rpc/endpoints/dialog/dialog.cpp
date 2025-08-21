module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Dialog;

import std;
import Core.State;
import Core.WebView.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Utils.Dialog;

namespace Core::RPC::Endpoints::Dialog {

auto handle_select_file([[maybe_unused]] Core::State::AppState& app_state,
                        const Utils::Dialog::FileSelectorParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Utils::Dialog::FileSelectorResult>> {
  auto result = params.parent_to_webview
                    ? Utils::Dialog::select_file(params, app_state.webview->window.webview_hwnd)
                    : Utils::Dialog::select_file(params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_select_folder([[maybe_unused]] Core::State::AppState& app_state,
                          const Utils::Dialog::FolderSelectorParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Utils::Dialog::FolderSelectorResult>> {
  auto result = params.parent_to_webview
                    ? Utils::Dialog::select_folder(params, app_state.webview->window.webview_hwnd)
                    : Utils::Dialog::select_folder(params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<Utils::Dialog::FileSelectorParams, Utils::Dialog::FileSelectorResult>(
      app_state, app_state.rpc->registry, "file_dialog.select_file", handle_select_file,
      "Open a file picker and return selected file paths");

  Core::RPC::register_method<Utils::Dialog::FolderSelectorParams,
                             Utils::Dialog::FolderSelectorResult>(
      app_state, app_state.rpc->registry, "file_dialog.select_folder", handle_select_folder,
      "Open a folder picker and return selected path");
}

}  // namespace Core::RPC::Endpoints::Dialog