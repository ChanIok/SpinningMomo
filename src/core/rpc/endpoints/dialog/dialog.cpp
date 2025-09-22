module;

#include <asio.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Dialog;

import std;
import Vendor.Windows;
import Core.State;
import Core.WebView.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Utils.Dialog;

namespace Core::RPC::Endpoints::Dialog {

// 获取父窗口句柄的辅助函数
auto get_parent_window(Core::State::AppState& app_state, int8_t mode) -> Vendor::Windows::HWND {
  switch (mode) {
    case 0:  // 无父窗口
      return nullptr;
    case 1:  // webview2
      return app_state.webview->window.webview_hwnd;
    case 2:  // 激活窗口
      return Vendor::Windows::GetForegroundWindow();
    default:
      return nullptr;  // 默认无父窗口
  }
}

auto handle_select_file([[maybe_unused]] Core::State::AppState& app_state,
                        const Utils::Dialog::FileSelectorParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Utils::Dialog::FileSelectorResult>> {
  Vendor::Windows::HWND hwnd = get_parent_window(app_state, params.parent_window_mode);
  auto result = Utils::Dialog::select_file(params, hwnd);
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
  Vendor::Windows::HWND hwnd = get_parent_window(app_state, params.parent_window_mode);
  auto result = Utils::Dialog::select_folder(params, hwnd);
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
      app_state, app_state.rpc->registry, "dialog.openFile", handle_select_file,
      "Open a file picker and return selected file paths");

  Core::RPC::register_method<Utils::Dialog::FolderSelectorParams,
                             Utils::Dialog::FolderSelectorResult>(
      app_state, app_state.rpc->registry, "dialog.openDirectory", handle_select_folder,
      "Open a folder picker and return selected path");
}

}  // namespace Core::RPC::Endpoints::Dialog