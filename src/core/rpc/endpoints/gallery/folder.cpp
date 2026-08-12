#include "core/rpc/endpoints/gallery/folder.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/rpc/notification_hub.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/folder/repository.hpp"
#include "features/gallery/folder/service.hpp"
#include "features/gallery/types.hpp"

namespace core::rpc::endpoints::gallery::folder {

struct UpdateFolderDisplayNameParams {
  std::int64_t id;
  std::optional<std::string> display_name;
};

struct CreateFolderParams {
  std::int64_t parent_folder_id;
  std::string name;
};

// ============= 文件夹树 RPC 处理函数 =============

auto handle_get_folder_tree(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<features::gallery::FolderTreeNode>> {
  auto result = features::gallery::folder::repository::get_folder_tree(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// 在已索引父目录下创建真实子目录，并通知前端刷新文件夹树。
auto handle_create_folder(core::AppState& app_state, const CreateFolderParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto create_result = features::gallery::folder::service::create_child_folder(
      app_state, params.parent_folder_id, params.name);
  if (!create_result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + create_result.error()});
  }

  // 目录变化只刷新 Gallery UI，不构造文件级 ScanChange。
  core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  co_return create_result.value();
}

auto handle_update_folder_display_name(core::AppState& app_state,
                                       const UpdateFolderDisplayNameParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto update_result = features::gallery::folder::service::update_folder_display_name(
      app_state, params.id, params.display_name);
  if (!update_result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + update_result.error()});
  }

  co_return update_result.value();
}

auto handle_open_folder_in_explorer(core::AppState& app_state,
                                    const features::gallery::GetParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto open_result =
      features::gallery::folder::service::open_folder_in_explorer(app_state, params.id);
  if (!open_result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + open_result.error()});
  }

  co_return open_result.value();
}

auto handle_remove_folder_watch(core::AppState& app_state,
                                const features::gallery::GetParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto remove_result =
      features::gallery::folder::service::remove_root_folder_watch(app_state, params.id);
  if (!remove_result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + remove_result.error()});
  }

  core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  co_return remove_result.value();
}

// 注册文件夹浏览、编辑和宿主文件夹操作，并明确区分 LAN 与 local 权限。

auto register_all(core::AppState& app_state) -> void {
  // 文件夹树
  register_method<EmptyParams, std::vector<features::gallery::FolderTreeNode>>(
      app_state, app_state.rpc->registry, "gallery.getFolderTree", handle_get_folder_tree,
      "Get folder tree structure for navigation", AccessLevel::lan);

  register_method<CreateFolderParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.createFolder", handle_create_folder,
      "Create a physical child folder and index it immediately", AccessLevel::lan);

  register_method<UpdateFolderDisplayNameParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateFolderDisplayName",
      handle_update_folder_display_name, "Update folder display name", AccessLevel::lan);

  // 打开资源管理器和移除监听会操作宿主机文件系统，仅允许 local 调用。
  register_method<features::gallery::GetParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.openFolderInExplorer",
      handle_open_folder_in_explorer, "Open folder in explorer");

  register_method<features::gallery::GetParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.removeFolderWatch", handle_remove_folder_watch,
      "Remove root folder watch and clean gallery index");
}

}  // namespace core::rpc::endpoints::gallery::folder
