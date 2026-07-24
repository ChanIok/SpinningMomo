#include "core/rpc/endpoints/gallery/asset.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/rpc/notification_hub.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/asset/repository.hpp"
#include "features/gallery/asset/service.hpp"
#include "features/gallery/clipboard/clipboard.hpp"
#include "features/gallery/file_operations/file_operations.hpp"
#include "features/gallery/original_locator.hpp"
#include "features/gallery/root_availability.hpp"
#include "features/gallery/types.hpp"

namespace core::rpc::endpoints::gallery::asset {

struct CheckAssetReachableParams {
  std::int64_t asset_id = 0;
};

struct PasteClipboardToFolderParams {
  std::int64_t folder_id = 0;
};

struct CheckAssetReachableResult {
  bool exists = false;
  bool readable = false;
  std::optional<std::string> path;
  std::optional<std::string> reason;
};

// ============= 时间线视图 RPC 处理函数 =============

auto handle_get_timeline_buckets(core::AppState& app_state,
                                 const features::gallery::TimelineBucketsParams& params)
    -> RpcAwaitable<features::gallery::TimelineBucketsResponse> {
  auto result = features::gallery::asset::service::get_timeline_buckets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_assets_by_month(core::AppState& app_state,
                                const features::gallery::GetAssetsByMonthParams& params)
    -> RpcAwaitable<features::gallery::GetAssetsByMonthResponse> {
  auto result = features::gallery::asset::service::get_assets_by_month(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// ============= 统一查询 RPC 处理函数 =============

auto handle_query_assets(core::AppState& app_state,
                         const features::gallery::QueryAssetsParams& params)
    -> RpcAwaitable<features::gallery::ListResponse> {
  auto result = features::gallery::asset::service::query_assets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_query_asset_layout_meta(core::AppState& app_state,
                                    const features::gallery::QueryAssetLayoutMetaParams& params)
    -> RpcAwaitable<features::gallery::QueryAssetLayoutMetaResponse> {
  auto result = features::gallery::asset::service::query_asset_layout_meta(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_asset_main_colors(core::AppState& app_state,
                                  const features::gallery::GetAssetMainColorsParams& params)
    -> RpcAwaitable<std::vector<features::gallery::AssetMainColor>> {
  auto result = features::gallery::asset::service::get_asset_main_colors(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_home_stats(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<features::gallery::HomeStats> {
  auto result = features::gallery::asset::service::get_home_stats(app_state);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_batch_selection_summary(
    core::AppState& app_state, const features::gallery::BatchSelectionSummaryParams& params)
    -> RpcAwaitable<features::gallery::BatchSelectionSummary> {
  auto result = features::gallery::asset::service::get_batch_selection_summary(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_missing_assets(core::AppState& app_state,
                               [[maybe_unused]] const EmptyParams& params)
    -> RpcAwaitable<features::gallery::MissingAssetsResponse> {
  auto result = features::gallery::asset::service::get_missing_assets(app_state);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }
  co_return result.value();
}

auto handle_purge_missing_assets(core::AppState& app_state,
                                 const features::gallery::PurgeMissingAssetsParams& params)
    -> RpcAwaitable<features::gallery::PurgeMissingAssetsResult> {
  auto result = features::gallery::asset::service::purge_missing_assets(app_state, params);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->deleted_asset_count > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }
  co_return result.value();
}

// ============= 资产动作 RPC 处理函数 =============

auto handle_open_asset_default(core::AppState& app_state,
                               const features::gallery::GetParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result =
      features::gallery::file_operations::open_asset_with_default_app(app_state, params.id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_reveal_asset_in_explorer(core::AppState& app_state,
                                     const features::gallery::GetParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::file_operations::reveal_asset_in_explorer(app_state, params.id);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_copy_assets_to_clipboard(core::AppState& app_state,
                                     const features::gallery::AssetIdsParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::clipboard::copy_assets(app_state, params.ids);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

// 将系统剪贴板中的文件或截图导入指定的已索引图库文件夹。
auto handle_paste_clipboard_to_folder(core::AppState& app_state,
                                      const PasteClipboardToFolderParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::clipboard::paste_to_folder(app_state, params.folder_id);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }
  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }
  co_return result.value();
}

auto handle_delete_assets(core::AppState& app_state,
                          const features::gallery::DeleteAssetsParams& params)
    -> RpcAwaitable<features::gallery::DeleteAssetsResult> {
  auto result = features::gallery::file_operations::delete_assets(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_move_assets_to_folder(core::AppState& app_state,
                                  const features::gallery::MoveAssetsToFolderParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::file_operations::move_assets_to_folder(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_update_assets_review_state(
    core::AppState& app_state, const features::gallery::UpdateAssetsReviewStateParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::asset::service::update_assets_review_state(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_update_asset_description(core::AppState& app_state,
                                     const features::gallery::UpdateAssetDescriptionParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::asset::service::update_asset_description(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_update_assets_description(
    core::AppState& app_state, const features::gallery::UpdateAssetsDescriptionParams& params)
    -> RpcAwaitable<features::gallery::OperationResult> {
  auto result = features::gallery::asset::service::update_assets_description(app_state, params);

  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Service error: " + result.error()});
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_check_asset_reachable(core::AppState& app_state,
                                  const CheckAssetReachableParams& params)
    -> RpcAwaitable<CheckAssetReachableResult> {
  if (params.asset_id <= 0) {
    co_return std::unexpected(RpcError{
        .code = static_cast<int>(ErrorCode::InvalidParams),
        .message = "assetId must be greater than 0",
    });
  }

  auto asset_result =
      features::gallery::asset::repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    co_return std::unexpected(RpcError{
        .code = static_cast<int>(ErrorCode::ServerError),
        .message = "Failed to query asset: " + asset_result.error(),
    });
  }

  if (!asset_result->has_value()) {
    co_return CheckAssetReachableResult{
        .exists = false,
        .readable = false,
        .path = std::nullopt,
        .reason = std::string("Asset not found in index"),
    };
  }

  auto asset = asset_result->value();
  std::vector<features::gallery::Asset> assets{asset};
  auto locator_result =
      features::gallery::original_locator::populate_asset_locators(app_state, assets);
  if (locator_result && !assets.empty()) {
    asset = std::move(assets.front());
  }

  if (asset.root_id.has_value() &&
      features::gallery::root_availability::is_remote_unreachable(app_state, *asset.root_id)) {
    co_return CheckAssetReachableResult{
        .exists = false,
        .readable = false,
        .path = asset.path,
        .reason = std::string("Remote root is unavailable"),
    };
  }

  std::filesystem::path file_path(asset.path);
  std::error_code ec;
  const bool exists = std::filesystem::exists(file_path, ec) && !ec;

  if (!exists) {
    co_return CheckAssetReachableResult{
        .exists = false,
        .readable = false,
        .path = asset.path,
        .reason = std::string("File not found on disk"),
    };
  }

  std::ifstream stream(file_path, std::ios::binary);
  if (!stream.is_open()) {
    co_return CheckAssetReachableResult{
        .exists = true,
        .readable = false,
        .path = asset.path,
        .reason = std::string("Failed to open file for reading"),
    };
  }

  co_return CheckAssetReachableResult{
      .exists = true,
      .readable = true,
      .path = asset.path,
      .reason = std::nullopt,
  };
}

// ============= RPC 方法注册 =============

auto register_all(core::AppState& app_state) -> void {
  // 时间线视图
  register_method<features::gallery::TimelineBucketsParams,
                  features::gallery::TimelineBucketsResponse>(
      app_state, app_state.rpc->registry, "gallery.getTimelineBuckets", handle_get_timeline_buckets,
      "Get timeline buckets (months) with asset counts for timeline view");

  register_method<features::gallery::GetAssetsByMonthParams,
                  features::gallery::GetAssetsByMonthResponse>(
      app_state, app_state.rpc->registry, "gallery.getAssetsByMonth", handle_get_assets_by_month,
      "Get all assets for a specific month in timeline view");

  // 统一资产查询接口
  register_method<features::gallery::QueryAssetsParams, features::gallery::ListResponse>(
      app_state, app_state.rpc->registry, "gallery.queryAssets", handle_query_assets,
      "Unified asset query interface with flexible filters (folder, month, year, type, search) "
      "and optional pagination");

  register_method<features::gallery::QueryAssetLayoutMetaParams,
                  features::gallery::QueryAssetLayoutMetaResponse>(
      app_state, app_state.rpc->registry, "gallery.queryAssetLayoutMeta",
      handle_query_asset_layout_meta,
      "Query lightweight asset layout metadata for adaptive gallery layout calculation");

  register_method<features::gallery::GetAssetMainColorsParams,
                  std::vector<features::gallery::AssetMainColor>>(
      app_state, app_state.rpc->registry, "gallery.getAssetMainColors",
      handle_get_asset_main_colors, "Get extracted main colors for the specified asset");

  register_method<EmptyParams, features::gallery::HomeStats>(
      app_state, app_state.rpc->registry, "gallery.getHomeStats", handle_get_home_stats,
      "Get home page gallery stats summary");

  register_method<features::gallery::BatchSelectionSummaryParams,
                  features::gallery::BatchSelectionSummary>(
      app_state, app_state.rpc->registry, "gallery.getBatchSelectionSummary",
      handle_get_batch_selection_summary,
      "Get the aggregated review and common-tag summary for the current selection");

  register_method<EmptyParams, features::gallery::MissingAssetsResponse>(
      app_state, app_state.rpc->registry, "gallery.getMissingAssets", handle_get_missing_assets,
      "List assets in the missing recovery period and their reclaimable thumbnail storage");

  register_method<features::gallery::PurgeMissingAssetsParams,
                  features::gallery::PurgeMissingAssetsResult>(
      app_state, app_state.rpc->registry, "gallery.purgeMissingAssets", handle_purge_missing_assets,
      "Permanently purge selected or all assets that are still marked missing");

  register_method<features::gallery::GetParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.openAssetDefault", handle_open_asset_default,
      "Open the selected asset file with the default system application");

  register_method<features::gallery::GetParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.revealAssetInExplorer",
      handle_reveal_asset_in_explorer, "Reveal and select the asset file in explorer");

  register_method<features::gallery::AssetIdsParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.copyAssetsToClipboard",
      handle_copy_assets_to_clipboard,
      "Copy selected asset files to the system clipboard as files");

  register_method<PasteClipboardToFolderParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.pasteClipboardToFolder",
      handle_paste_clipboard_to_folder,
      "Paste clipboard files or bitmap media into an indexed gallery folder");

  register_method<features::gallery::DeleteAssetsParams, features::gallery::DeleteAssetsResult>(
      app_state, app_state.rpc->registry, "gallery.deleteAssets", handle_delete_assets,
      "Recycle selected asset files where possible or permanently delete them");

  register_method<features::gallery::MoveAssetsToFolderParams, features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.moveAssetsToFolder",
      handle_move_assets_to_folder,
      "Move selected assets to an indexed target folder and update gallery index paths");

  register_method<features::gallery::UpdateAssetsReviewStateParams,
                  features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateAssetsReviewState",
      handle_update_assets_review_state,
      "Batch update Lightroom-style review metadata such as rating and pick/reject state");

  register_method<features::gallery::UpdateAssetDescriptionParams,
                  features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateAssetDescription",
      handle_update_asset_description,
      "Update a single asset description in the gallery details panel");

  register_method<features::gallery::UpdateAssetsDescriptionParams,
                  features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "gallery.updateAssetsDescription",
      handle_update_assets_description,
      "Batch update selected assets description in the gallery details panel");

  register_method<CheckAssetReachableParams, CheckAssetReachableResult>(
      app_state, app_state.rpc->registry, "gallery.checkAssetReachable",
      handle_check_asset_reachable,
      "Check whether an indexed asset file still exists and is readable on disk");
}

}  // namespace core::rpc::endpoints::gallery::asset
