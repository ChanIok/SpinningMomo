module;

#include <asio.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.Gallery;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Gallery;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Tag.Repository;

namespace Core::RPC::Endpoints::Gallery {

// ============= 扫描和索引 RPC 处理函数 =============

auto handle_scan_directory(Core::State::AppState& app_state,
                           const Features::Gallery::Types::ScanOptions& options)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::ScanResult>> {
  auto result = Features::Gallery::scan_directory(app_state, options);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 缩略图 RPC 处理函数 =============

auto handle_cleanup_thumbnails(Core::State::AppState& app_state,
                               [[maybe_unused]] const Core::RPC::EmptyParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::cleanup_thumbnails(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 统计和信息 RPC 处理函数 =============

auto handle_get_asset_stats(Core::State::AppState& app_state,
                            const Features::Gallery::Types::GetStatsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::Stats>> {
  auto result = Features::Gallery::get_asset_stats(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_thumbnail_stats(Core::State::AppState& app_state,
                                [[maybe_unused]] const Core::RPC::EmptyParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::string>> {
  auto result = Features::Gallery::get_thumbnail_stats(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 维护和优化 RPC 处理函数 =============

struct CleanupDeletedParams {
  std::optional<int> days_old = 30;
};

auto handle_cleanup_deleted(Core::State::AppState& app_state, const CleanupDeletedParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::cleanup_deleted_assets(app_state, params.days_old.value_or(30));

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 文件夹树 RPC 处理函数 =============

auto handle_get_folder_tree(Core::State::AppState& app_state,
                            [[maybe_unused]] const Core::RPC::EmptyParams& params)
    -> asio::awaitable<
        Core::RPC::RpcResult<std::vector<Features::Gallery::Types::FolderTreeNode>>> {
  auto result = Features::Gallery::Folder::Repository::get_folder_tree(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 时间线视图 RPC 处理函数 =============

auto handle_get_timeline_buckets(Core::State::AppState& app_state,
                                 const Features::Gallery::Types::TimelineBucketsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::TimelineBucketsResponse>> {
  auto result = Features::Gallery::Asset::Repository::get_timeline_buckets(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_assets_by_month(Core::State::AppState& app_state,
                                const Features::Gallery::Types::GetAssetsByMonthParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::GetAssetsByMonthResponse>> {
  auto result = Features::Gallery::Asset::Repository::get_assets_by_month(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 统一查询 RPC 处理函数 =============

auto handle_query_assets(Core::State::AppState& app_state,
                         const Features::Gallery::Types::QueryAssetsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::ListResponse>> {
  auto result = Features::Gallery::Asset::Repository::query_assets(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 标签管理 RPC 处理函数 =============

auto handle_list_tags(Core::State::AppState& app_state,
                      [[maybe_unused]] const Core::RPC::EmptyParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::vector<Features::Gallery::Types::Tag>>> {
  auto result = Features::Gallery::Tag::Repository::list_all_tags(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_tag_tree(Core::State::AppState& app_state,
                         [[maybe_unused]] const Core::RPC::EmptyParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::vector<Features::Gallery::Types::TagTreeNode>>> {
  auto result = Features::Gallery::Tag::Repository::get_tag_tree(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_create_tag(Core::State::AppState& app_state,
                       const Features::Gallery::Types::CreateTagParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::int64_t>> {
  auto result = Features::Gallery::Tag::Repository::create_tag(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_update_tag(Core::State::AppState& app_state,
                       const Features::Gallery::Types::UpdateTagParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::Tag::Repository::update_tag(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{.success = true,
                                                      .message = "Tag updated successfully"};
}

auto handle_delete_tag(Core::State::AppState& app_state,
                       const Features::Gallery::Types::GetParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::Tag::Repository::delete_tag(app_state, params.id);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{.success = true,
                                                      .message = "Tag deleted successfully"};
}

auto handle_get_tag_stats(Core::State::AppState& app_state,
                          [[maybe_unused]] const Core::RPC::EmptyParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::vector<Features::Gallery::Types::TagStats>>> {
  auto result = Features::Gallery::Tag::Repository::get_tag_stats(app_state);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 资产-标签关联 RPC 处理函数 =============

auto handle_add_tags_to_asset(Core::State::AppState& app_state,
                              const Features::Gallery::Types::AddTagsToAssetParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::Tag::Repository::add_tags_to_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{
      .success = true, .message = "Tags added to asset successfully"};
}

auto handle_remove_tags_from_asset(
    Core::State::AppState& app_state,
    const Features::Gallery::Types::RemoveTagsFromAssetParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Gallery::Types::OperationResult>> {
  auto result = Features::Gallery::Tag::Repository::remove_tags_from_asset(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return Features::Gallery::Types::OperationResult{
      .success = true, .message = "Tags removed from asset successfully"};
}

auto handle_get_asset_tags(Core::State::AppState& app_state,
                           const Features::Gallery::Types::GetAssetTagsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<std::vector<Features::Gallery::Types::Tag>>> {
  auto result = Features::Gallery::Tag::Repository::get_asset_tags(app_state, params.asset_id);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

struct GetTagsByAssetIdsParams {
  std::vector<std::int64_t> asset_ids;
};

auto handle_get_tags_by_asset_ids(Core::State::AppState& app_state,
                                  const GetTagsByAssetIdsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<
        std::unordered_map<std::int64_t, std::vector<Features::Gallery::Types::Tag>>>> {
  auto result =
      Features::Gallery::Tag::Repository::get_tags_by_asset_ids(app_state, params.asset_ids);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= RPC 方法注册 =============

auto register_all(Core::State::AppState& app_state) -> void {
  // 基础资产项操作

  // 扫描和索引
  Core::RPC::register_method<Features::Gallery::Types::ScanOptions,
                             Features::Gallery::Types::ScanResult>(
      app_state, app_state.rpc->registry, "gallery.scanDirectory", handle_scan_directory,
      "Scan directory for asset files and add them to the library. Supports ignore rules and "
      "folder management.");

  // 缩略图操作
  Core::RPC::register_method<Core::RPC::EmptyParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.cleanupThumbnails", handle_cleanup_thumbnails,
      "Clean up orphaned thumbnail files");

  // 统计信息
  Core::RPC::register_method<Features::Gallery::Types::GetStatsParams,
                             Features::Gallery::Types::Stats>(
      app_state, app_state.rpc->registry, "gallery.stats", handle_get_asset_stats,
      "Get asset library statistics");

  Core::RPC::register_method<Core::RPC::EmptyParams, std::string>(
      app_state, app_state.rpc->registry, "gallery.thumbnailStats", handle_get_thumbnail_stats,
      "Get thumbnail storage statistics");

  // 维护操作
  Core::RPC::register_method<CleanupDeletedParams, Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.cleanupDeleted", handle_cleanup_deleted,
      "Clean up soft-deleted items older than specified days");

  // 文件夹树
  Core::RPC::register_method<Core::RPC::EmptyParams,
                             std::vector<Features::Gallery::Types::FolderTreeNode>>(
      app_state, app_state.rpc->registry, "gallery.getFolderTree", handle_get_folder_tree,
      "Get folder tree structure for navigation");

  // 时间线视图
  Core::RPC::register_method<Features::Gallery::Types::TimelineBucketsParams,
                             Features::Gallery::Types::TimelineBucketsResponse>(
      app_state, app_state.rpc->registry, "gallery.getTimelineBuckets", handle_get_timeline_buckets,
      "Get timeline buckets (months) with asset counts for timeline view");

  Core::RPC::register_method<Features::Gallery::Types::GetAssetsByMonthParams,
                             Features::Gallery::Types::GetAssetsByMonthResponse>(
      app_state, app_state.rpc->registry, "gallery.getAssetsByMonth", handle_get_assets_by_month,
      "Get all assets for a specific month in timeline view");

  // 统一资产查询接口
  Core::RPC::register_method<Features::Gallery::Types::QueryAssetsParams,
                             Features::Gallery::Types::ListResponse>(
      app_state, app_state.rpc->registry, "gallery.queryAssets", handle_query_assets,
      "Unified asset query interface with flexible filters (folder, month, year, type, search) "
      "and optional pagination");

  // 标签管理
  Core::RPC::register_method<Core::RPC::EmptyParams, std::vector<Features::Gallery::Types::Tag>>(
      app_state, app_state.rpc->registry, "gallery.listTags", handle_list_tags,
      "Get all tags as a flat list");

  Core::RPC::register_method<Core::RPC::EmptyParams,
                             std::vector<Features::Gallery::Types::TagTreeNode>>(
      app_state, app_state.rpc->registry, "gallery.getTagTree", handle_get_tag_tree,
      "Get tag tree structure for navigation");

  Core::RPC::register_method<Features::Gallery::Types::CreateTagParams, std::int64_t>(
      app_state, app_state.rpc->registry, "gallery.createTag", handle_create_tag,
      "Create a new tag");

  Core::RPC::register_method<Features::Gallery::Types::UpdateTagParams,
                             Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateTag", handle_update_tag,
      "Update an existing tag");

  Core::RPC::register_method<Features::Gallery::Types::GetParams,
                             Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.deleteTag", handle_delete_tag,
      "Delete a tag and its associations");

  Core::RPC::register_method<Core::RPC::EmptyParams,
                             std::vector<Features::Gallery::Types::TagStats>>(
      app_state, app_state.rpc->registry, "gallery.getTagStats", handle_get_tag_stats,
      "Get tag usage statistics");

  // 资产-标签关联
  Core::RPC::register_method<Features::Gallery::Types::AddTagsToAssetParams,
                             Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.addTagsToAsset", handle_add_tags_to_asset,
      "Add tags to an asset");

  Core::RPC::register_method<Features::Gallery::Types::RemoveTagsFromAssetParams,
                             Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.removeTagsFromAsset",
      handle_remove_tags_from_asset, "Remove tags from an asset");

  Core::RPC::register_method<Features::Gallery::Types::GetAssetTagsParams,
                             std::vector<Features::Gallery::Types::Tag>>(
      app_state, app_state.rpc->registry, "gallery.getAssetTags", handle_get_asset_tags,
      "Get all tags for a specific asset");

  Core::RPC::register_method<
      GetTagsByAssetIdsParams,
      std::unordered_map<std::int64_t, std::vector<Features::Gallery::Types::Tag>>>(
      app_state, app_state.rpc->registry, "gallery.getTagsByAssetIds", handle_get_tags_by_asset_ids,
      "Batch get tags for multiple assets");
}

}  // namespace Core::RPC::Endpoints::Gallery
