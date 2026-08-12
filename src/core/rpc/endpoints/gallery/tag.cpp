#include "core/rpc/endpoints/gallery/tag.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/rpc/notification_hub.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/tag/repository.hpp"
#include "features/gallery/types.hpp"

namespace core::rpc::endpoints::gallery::tag {

struct GetTagsByAssetIdsParams {
  std::vector<std::int64_t> asset_ids;
};

// ============= 标签管理 RPC 处理函数 =============

auto handle_get_tag_tree(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<features::gallery::TagTreeNode>> {
  auto result = features::gallery::tag::repository::get_tag_tree(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_create_tag(core::AppState& app_state, const features::gallery::CreateTagParams& params)
    -> RpcAwaitable<std::int64_t> {
  auto result = features::gallery::tag::repository::create_tag(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_update_tag(core::AppState& app_state, const features::gallery::UpdateTagParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::tag::repository::update_tag(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return features::gallery::OperationResult{.success = true,
                                               .message = "Tag updated successfully"};
}

auto handle_delete_tag(core::AppState& app_state, const features::gallery::GetParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::tag::repository::delete_tag(app_state, params.id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return features::gallery::OperationResult{.success = true,
                                               .message = "Tag deleted successfully"};
}

auto handle_get_tag_stats(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<std::vector<features::gallery::TagStats>> {
  auto result = features::gallery::tag::repository::get_tag_stats(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 资产-标签关联 RPC 处理函数 =============

auto handle_add_tags_to_asset(core::AppState& app_state,
                              const features::gallery::AddTagsToAssetParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::tag::repository::add_tags_to_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return features::gallery::OperationResult{.success = true,
                                               .message = "Tags added to asset successfully"};
}

auto handle_add_tag_to_assets(core::AppState& app_state,
                              const features::gallery::AddTagToAssetsParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::tag::repository::add_tag_to_assets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_remove_tag_from_assets(core::AppState& app_state,
                                   const features::gallery::RemoveTagFromAssetsParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::tag::repository::remove_tag_from_assets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_remove_tags_from_asset(core::AppState& app_state,
                                   const features::gallery::RemoveTagsFromAssetParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::tag::repository::remove_tags_from_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return features::gallery::OperationResult{.success = true,
                                               .message = "Tags removed from asset successfully"};
}

auto handle_get_asset_tags(core::AppState& app_state,
                           const features::gallery::GetAssetTagsParams& params)
    -> RpcAwaitable<std::vector<features::gallery::Tag>> {
  auto result = features::gallery::tag::repository::get_asset_tags(app_state, params.asset_id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_tags_by_asset_ids(core::AppState& app_state, const GetTagsByAssetIdsParams& params)
    -> RpcAwaitable<std::unordered_map<std::int64_t, std::vector<features::gallery::Tag>>> {
  auto result =
      features::gallery::tag::repository::get_tags_by_asset_ids(app_state, params.asset_ids);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(core::AppState& app_state) -> void {
  // 标签树、标签统计和标签关联属于图库管理能力，允许 LAN 调用。
  // 标签管理
  register_method<EmptyParams, std::vector<features::gallery::TagTreeNode>>(
      app_state, app_state.rpc->registry, "gallery.getTagTree", handle_get_tag_tree,
      "Get tag tree structure for navigation", AccessLevel::lan);

  register_method<features::gallery::CreateTagParams, std::int64_t>(
      app_state, app_state.rpc->registry, "gallery.createTag", handle_create_tag,
      "Create a new tag", AccessLevel::lan);

  register_method<features::gallery::UpdateTagParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateTag", handle_update_tag,
      "Update an existing tag", AccessLevel::lan);

  register_method<features::gallery::GetParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.deleteTag", handle_delete_tag,
      "Delete a tag and its associations", AccessLevel::lan);

  register_method<EmptyParams, std::vector<features::gallery::TagStats>>(
      app_state, app_state.rpc->registry, "gallery.getTagStats", handle_get_tag_stats,
      "Get tag usage statistics", AccessLevel::lan);

  // 资产-标签关联
  register_method<features::gallery::AddTagsToAssetParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.addTagsToAsset", handle_add_tags_to_asset,
      "Add tags to an asset", AccessLevel::lan);

  register_method<features::gallery::AddTagToAssetsParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.addTagToAssets", handle_add_tag_to_assets,
      "Add a tag to multiple assets", AccessLevel::lan);

  register_method<features::gallery::RemoveTagFromAssetsParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.removeTagFromAssets",
      handle_remove_tag_from_assets, "Remove a tag from multiple assets", AccessLevel::lan);

  register_method<features::gallery::RemoveTagsFromAssetParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.removeTagsFromAsset",
      handle_remove_tags_from_asset, "Remove tags from an asset", AccessLevel::lan);

  register_method<features::gallery::GetAssetTagsParams, std::vector<features::gallery::Tag>>(
      app_state, app_state.rpc->registry, "gallery.getAssetTags", handle_get_asset_tags,
      "Get all tags for a specific asset", AccessLevel::lan);

  register_method<GetTagsByAssetIdsParams,
                  std::unordered_map<std::int64_t, std::vector<features::gallery::Tag>>>(
      app_state, app_state.rpc->registry, "gallery.getTagsByAssetIds", handle_get_tags_by_asset_ids,
      "Batch get tags for multiple assets", AccessLevel::lan);
}

}  // namespace core::rpc::endpoints::gallery::tag
