#include "core/rpc/endpoints/dialog/dialog.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/windows.hpp"

#include "core/dialog_service/dialog_service.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "core/webview/state.hpp"
#include "utils/dialog/dialog.hpp"

namespace core::rpc::endpoints::dialog {

// 获取父窗口句柄的辅助函数
auto get_parent_window(core::AppState& app_state, int8_t mode) -> HWND {
  switch (mode) {
    case 0:  // 无父窗口
      return nullptr;
    case 1:  // webview2
      return app_state.webview->window.webview_hwnd;
    case 2:  // 激活窗口
      return GetForegroundWindow();
    default:
      return nullptr;  // 默认无父窗口
  }
}

auto handle_select_file([[maybe_unused]] core::AppState& app_state,
                        const utils::dialog::FileSelectorParams& params)
    -> asio::awaitable<core::rpc::RpcResult<utils::dialog::FileSelectorResult>> {
  HWND hwnd = get_parent_window(app_state, params.parent_window_mode);
  auto result = core::dialog_service::open_file(app_state, params, hwnd);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_select_folder([[maybe_unused]] core::AppState& app_state,
                          const utils::dialog::FolderSelectorParams& params)
    -> asio::awaitable<core::rpc::RpcResult<utils::dialog::FolderSelectorResult>> {
  HWND hwnd = get_parent_window(app_state, params.parent_window_mode);
  auto result = core::dialog_service::open_folder(app_state, params, hwnd);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<utils::dialog::FileSelectorParams, utils::dialog::FileSelectorResult>(
      app_state, app_state.rpc->registry, "dialog.openFile", handle_select_file,
      "Open a file picker and return selected file paths");

  core::rpc::register_method<utils::dialog::FolderSelectorParams,
                             utils::dialog::FolderSelectorResult>(
      app_state, app_state.rpc->registry, "dialog.openDirectory", handle_select_folder,
      "Open a folder picker and return selected path");
}

}  // namespace core::rpc::endpoints::dialog
