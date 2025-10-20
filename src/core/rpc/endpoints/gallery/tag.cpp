module;

module Core.RPC.Endpoints.Gallery.Tag;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Gallery.Types;
import Features.Gallery.Tag.Repository;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Gallery::Tag {

struct GetTagsByAssetIdsParams {
  std::vector<std::int64_t> asset_ids;
};

// ============= 标签管理 RPC 处理函数 =============

auto handle_get_tag_tree(Core::State::AppState& app_state,
                         [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<Features::Gallery::Types::TagTreeNode>> {
  auto result = Features::Gallery::Tag::Repository::get_tag_tree(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_create_tag(Core::State::AppState& app_state,
                       const Features::Gallery::Types::CreateTagParams& params)
    -> RpcAwaitable<std::int64_t> {
  auto result = Features::Gallery::Tag::Repository::create_tag(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_update_tag(Core::State::AppState& app_state,
                       const Features::Gallery::Types::UpdateTagParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::Tag::Repository::update_tag(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{.success = true,
                                                      .message = "Tag updated successfully"};
}

auto handle_delete_tag(Core::State::AppState& app_state,
                       const Features::Gallery::Types::GetParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::Tag::Repository::delete_tag(app_state, params.id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{.success = true,
                                                      .message = "Tag deleted successfully"};
}

auto handle_get_tag_stats(Core::State::AppState& app_state,
                          [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<Features::Gallery::Types::TagStats>> {
  auto result = Features::Gallery::Tag::Repository::get_tag_stats(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 资产-标签关联 RPC 处理函数 =============

auto handle_add_tags_to_asset(Core::State::AppState& app_state,
                              const Features::Gallery::Types::AddTagsToAssetParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::Tag::Repository::add_tags_to_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{
      .success = true, .message = "Tags added to asset successfully"};
}

auto handle_remove_tags_from_asset(
    Core::State::AppState& app_state,
    const Features::Gallery::Types::RemoveTagsFromAssetParams& params)
    -> RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = Features::Gallery::Tag::Repository::remove_tags_from_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{
      .success = true, .message = "Tags removed from asset successfully"};
}

auto handle_get_asset_tags(Core::State::AppState& app_state,
                           const Features::Gallery::Types::GetAssetTagsParams& params)
    -> RpcAwaitable<std::vector<Features::Gallery::Types::Tag>> {
  auto result = Features::Gallery::Tag::Repository::get_asset_tags(app_state, params.asset_id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_tags_by_asset_ids(Core::State::AppState& app_state,
                                  const GetTagsByAssetIdsParams& params)
    -> RpcAwaitable<std::unordered_map<std::int64_t, std::vector<Features::Gallery::Types::Tag>>> {
  auto result =
      Features::Gallery::Tag::Repository::get_tags_by_asset_ids(app_state, params.asset_ids);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 标签管理
  register_method<EmptyParams, std::vector<Features::Gallery::Types::TagTreeNode>>(
      app_state, app_state.rpc->registry, "gallery.getTagTree", handle_get_tag_tree,
      "Get tag tree structure for navigation");

  register_method<Features::Gallery::Types::CreateTagParams, std::int64_t>(
      app_state, app_state.rpc->registry, "gallery.createTag", handle_create_tag,
      "Create a new tag");

  register_method<Features::Gallery::Types::UpdateTagParams,
                  Features::Gallery::Types::OperationResult>(app_state, app_state.rpc->registry,
                                                             "gallery.updateTag", handle_update_tag,
                                                             "Update an existing tag");

  register_method<Features::Gallery::Types::GetParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.deleteTag", handle_delete_tag,
      "Delete a tag and its associations");

  register_method<EmptyParams, std::vector<Features::Gallery::Types::TagStats>>(
      app_state, app_state.rpc->registry, "gallery.getTagStats", handle_get_tag_stats,
      "Get tag usage statistics");

  // 资产-标签关联
  register_method<Features::Gallery::Types::AddTagsToAssetParams,
                  Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.addTagsToAsset", handle_add_tags_to_asset,
      "Add tags to an asset");

  register_method<Features::Gallery::Types::RemoveTagsFromAssetParams,
                  Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.removeTagsFromAsset",
      handle_remove_tags_from_asset, "Remove tags from an asset");

  register_method<Features::Gallery::Types::GetAssetTagsParams,
                  std::vector<Features::Gallery::Types::Tag>>(
      app_state, app_state.rpc->registry, "gallery.getAssetTags", handle_get_asset_tags,
      "Get all tags for a specific asset");

  register_method<GetTagsByAssetIdsParams,
                  std::unordered_map<std::int64_t, std::vector<Features::Gallery::Types::Tag>>>(
      app_state, app_state.rpc->registry, "gallery.getTagsByAssetIds", handle_get_tags_by_asset_ids,
      "Batch get tags for multiple assets");
}

}  // namespace Core::RPC::Endpoints::Gallery::Tag
