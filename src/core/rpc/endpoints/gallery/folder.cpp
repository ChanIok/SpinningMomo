module;

module Core.RPC.Endpoints.Gallery.Folder;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.RPC.NotificationHub;
import Features.Gallery.Types;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Folder.Service;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Gallery::Folder {

struct UpdateFolderDisplayNameParams {
  std::int64_t id;
  std::optional<std::string> display_name;
};

// ============= 文件夹树 RPC 处理函数 =============

auto handle_get_folder_tree(Core::State::AppState& app_state,
                            [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<Features::Gallery::Types::FolderTreeNode>> {
  auto result = Features::Gallery::Folder::Repository::get_folder_tree(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_update_folder_display_name(Core::State::AppState& app_state,
                                       const UpdateFolderDisplayNameParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto update_result = Features::Gallery::Folder::Service::update_folder_display_name(
      app_state, params.id, params.display_name);
  if (!update_result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + update_result.error()});
  }

  co_return update_result.value();
}

auto handle_open_folder_in_explorer(Core::State::AppState& app_state,
                                    const Features::Gallery::Types::GetParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto open_result =
      Features::Gallery::Folder::Service::open_folder_in_explorer(app_state, params.id);
  if (!open_result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + open_result.error()});
  }

  co_return open_result.value();
}

auto handle_remove_folder_watch(Core::State::AppState& app_state,
                                const Features::Gallery::Types::GetParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto remove_result =
      Features::Gallery::Folder::Service::remove_root_folder_watch(app_state, params.id);
  if (!remove_result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + remove_result.error()});
  }

  Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
  co_return remove_result.value();
}

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 文件夹树
  register_method<EmptyParams, std::vector<Features::Gallery::Types::FolderTreeNode>>(
      app_state, app_state.rpc->registry, "gallery.getFolderTree", handle_get_folder_tree,
      "Get folder tree structure for navigation");

  register_method<UpdateFolderDisplayNameParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateFolderDisplayName",
      handle_update_folder_display_name, "Update folder display name");

  register_method<Features::Gallery::Types::GetParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.openFolderInExplorer",
      handle_open_folder_in_explorer, "Open folder in explorer");

  register_method<Features::Gallery::Types::GetParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.removeFolderWatch", handle_remove_folder_watch,
      "Remove root folder watch and clean gallery index");
}

}  // namespace Core::RPC::Endpoints::Gallery::Folder
