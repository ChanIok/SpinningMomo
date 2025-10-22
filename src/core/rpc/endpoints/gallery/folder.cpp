module;

module Core.RPC.Endpoints.Gallery.Folder;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Gallery.Types;
import Features.Gallery.Folder.Repository;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Gallery::Folder {

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

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 文件夹树
  register_method<EmptyParams, std::vector<Features::Gallery::Types::FolderTreeNode>>(
      app_state, app_state.rpc->registry, "gallery.getFolderTree", handle_get_folder_tree,
      "Get folder tree structure for navigation");
}

}  // namespace Core::RPC::Endpoints::Gallery::Folder
