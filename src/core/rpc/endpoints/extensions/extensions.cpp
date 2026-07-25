#include "core/rpc/endpoints/extensions/extensions.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/rfl.hpp"

#include "core/rpc/notification_hub.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "extensions/infinity_nikki/asset_service.hpp"
#include "extensions/infinity_nikki/game_directory.hpp"
#include "extensions/infinity_nikki/task_service.hpp"
#include "extensions/infinity_nikki/types.hpp"
#include "features/gallery/types.hpp"
#include "utils/logger/logger.hpp"

namespace core::rpc::endpoints::extensions {

struct StartExtensionTaskResult {
  std::string task_id;
};

auto handle_infinity_nikki_get_game_directory([[maybe_unused]] core::AppState& app_state,
                                              [[maybe_unused]] const rfl::Generic& params)
    -> core::rpc::RpcAwaitable<::extensions::infinity_nikki::InfinityNikkiGameDirResult> {
  auto result = ::extensions::infinity_nikki::game_directory::get_game_directory();
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Failed to get Infinity Nikki game directory: " + result.error(),
    });
  }

  co_return result.value();
}

auto handle_infinity_nikki_start_extract_photo_params(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::InfinityNikkiExtractPhotoParamsRequest& params)
    -> core::rpc::RpcAwaitable<StartExtensionTaskResult> {
  auto task_result = ::extensions::infinity_nikki::task_service::start_extract_photo_params_task(
      app_state, params);
  if (!task_result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::InvalidRequest),
        .message = task_result.error(),
    });
  }
  co_return StartExtensionTaskResult{.task_id = task_result.value()};
}

auto handle_infinity_nikki_start_extract_photo_params_for_folder(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::InfinityNikkiExtractPhotoParamsForFolderRequest& params)
    -> core::rpc::RpcAwaitable<StartExtensionTaskResult> {
  auto task_result =
      ::extensions::infinity_nikki::task_service::start_extract_photo_params_for_folder_task(
          app_state, params);
  if (!task_result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::InvalidRequest),
        .message = task_result.error(),
    });
  }
  co_return StartExtensionTaskResult{.task_id = task_result.value()};
}

auto handle_infinity_nikki_start_initialize_media_hardlinks(
    core::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> core::rpc::RpcAwaitable<StartExtensionTaskResult> {
  auto task_result =
      ::extensions::infinity_nikki::task_service::start_initialize_media_hardlinks_task(app_state);
  if (!task_result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::InvalidRequest),
        .message = task_result.error(),
    });
  }
  co_return StartExtensionTaskResult{.task_id = task_result.value()};
}

auto handle_infinity_nikki_query_photo_map_points(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::QueryPhotoMapPointsParams& params)
    -> core::rpc::RpcAwaitable<std::vector<::extensions::infinity_nikki::PhotoMapPoint>> {
  auto result = co_await ::extensions::infinity_nikki::asset_service::query_photo_map_points(
      app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_get_details(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::GetInfinityNikkiDetailsParams& params)
    -> core::rpc::RpcAwaitable<::extensions::infinity_nikki::InfinityNikkiDetails> {
  auto result =
      co_await ::extensions::infinity_nikki::asset_service::get_details(app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_get_dye_code_asset_ids(
    core::AppState& app_state, const ::extensions::infinity_nikki::GetDyeCodeAssetIdsParams& params)
    -> core::rpc::RpcAwaitable<std::vector<std::int64_t>> {
  auto result =
      ::extensions::infinity_nikki::asset_service::get_dye_code_asset_ids(app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_get_map_config(core::AppState& app_state,
                                          [[maybe_unused]] const rfl::Generic& params)
    -> core::rpc::RpcAwaitable<::extensions::infinity_nikki::InfinityNikkiMapConfig> {
  auto result = co_await ::extensions::infinity_nikki::asset_service::get_map_config(app_state);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_get_metadata_names(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::GetInfinityNikkiMetadataNamesParams& params)
    -> core::rpc::RpcAwaitable<::extensions::infinity_nikki::InfinityNikkiMetadataNames> {
  auto result =
      co_await ::extensions::infinity_nikki::asset_service::get_metadata_names(app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_set_user_record(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::SetInfinityNikkiUserRecordParams& params)
    -> core::rpc::RpcAwaitable<features::gallery::OperationResult> {
  auto result = ::extensions::infinity_nikki::asset_service::set_user_record(app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_infinity_nikki_preview_same_outfit_dye_code_fill(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::PreviewInfinityNikkiSameOutfitDyeCodeFillParams& params)
    -> core::rpc::RpcAwaitable<
        ::extensions::infinity_nikki::InfinityNikkiSameOutfitDyeCodeFillPreview> {
  auto result = ::extensions::infinity_nikki::asset_service::preview_same_outfit_dye_code_fill(
      app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_fill_same_outfit_dye_code(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::FillInfinityNikkiSameOutfitDyeCodeParams& params)
    -> core::rpc::RpcAwaitable<
        ::extensions::infinity_nikki::InfinityNikkiSameOutfitDyeCodeFillResult> {
  auto result =
      ::extensions::infinity_nikki::asset_service::fill_same_outfit_dye_code(app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }

  if (result->affected_count > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_infinity_nikki_set_world_record(
    core::AppState& app_state,
    const ::extensions::infinity_nikki::SetInfinityNikkiWorldRecordParams& params)
    -> core::rpc::RpcAwaitable<features::gallery::OperationResult> {
  auto result = ::extensions::infinity_nikki::asset_service::set_world_record(app_state, params);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }

  if (result->affected_count.value_or(0) > 0) {
    core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<rfl::Generic,
                             ::extensions::infinity_nikki::InfinityNikkiGameDirResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getGameDirectory",
      handle_infinity_nikki_get_game_directory,
      "Get Infinity Nikki game installation directory from launcher config");

  core::rpc::register_method<::extensions::infinity_nikki::InfinityNikkiExtractPhotoParamsRequest,
                             StartExtensionTaskResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.startExtractPhotoParams",
      handle_infinity_nikki_start_extract_photo_params,
      "Create a background task to extract and index Infinity Nikki photo params");

  core::rpc::register_method<
      ::extensions::infinity_nikki::InfinityNikkiExtractPhotoParamsForFolderRequest,
      StartExtensionTaskResult>(
      app_state, app_state.rpc->registry,
      "extensions.infinityNikki.startExtractPhotoParamsForFolder",
      handle_infinity_nikki_start_extract_photo_params_for_folder,
      "Create a background task to extract Infinity Nikki photo params for a gallery folder");

  core::rpc::register_method<rfl::Generic, StartExtensionTaskResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.startInitializeMediaHardlinks",
      handle_infinity_nikki_start_initialize_media_hardlinks,
      "Create a background task to initialize Infinity Nikki media hardlinks");

  core::rpc::register_method<::extensions::infinity_nikki::QueryPhotoMapPointsParams,
                             std::vector<::extensions::infinity_nikki::PhotoMapPoint>>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.queryPhotoMapPoints",
      handle_infinity_nikki_query_photo_map_points,
      "Query Infinity Nikki photo map points using the current gallery filters");

  core::rpc::register_method<::extensions::infinity_nikki::GetInfinityNikkiDetailsParams,
                             ::extensions::infinity_nikki::InfinityNikkiDetails>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getDetails",
      handle_infinity_nikki_get_details,
      "Get Infinity Nikki extracted data and user record for the specified asset");

  core::rpc::register_method<::extensions::infinity_nikki::GetDyeCodeAssetIdsParams,
                             std::vector<std::int64_t>>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getDyeCodeAssetIds",
      handle_infinity_nikki_get_dye_code_asset_ids,
      "Return which of the specified assets have a non-empty Infinity Nikki dye code");

  core::rpc::register_method<rfl::Generic, ::extensions::infinity_nikki::InfinityNikkiMapConfig>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getMapConfig",
      handle_infinity_nikki_get_map_config, "Get online Infinity Nikki map world configuration");

  core::rpc::register_method<::extensions::infinity_nikki::GetInfinityNikkiMetadataNamesParams,
                             ::extensions::infinity_nikki::InfinityNikkiMetadataNames>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getMetadataNames",
      handle_infinity_nikki_get_metadata_names,
      "Resolve localized names for Infinity Nikki metadata ids such as pose/filter/light");

  core::rpc::register_method<::extensions::infinity_nikki::SetInfinityNikkiUserRecordParams,
                             features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.setUserRecord",
      handle_infinity_nikki_set_user_record,
      "Set or clear a single Infinity Nikki user record in the gallery details panel");

  core::rpc::register_method<
      ::extensions::infinity_nikki::PreviewInfinityNikkiSameOutfitDyeCodeFillParams,
      ::extensions::infinity_nikki::InfinityNikkiSameOutfitDyeCodeFillPreview>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.previewSameOutfitDyeCodeFill",
      handle_infinity_nikki_preview_same_outfit_dye_code_fill,
      "Preview how many same Infinity Nikki outfit and dye assets can receive the current dye "
      "code");

  core::rpc::register_method<
      ::extensions::infinity_nikki::FillInfinityNikkiSameOutfitDyeCodeParams,
      ::extensions::infinity_nikki::InfinityNikkiSameOutfitDyeCodeFillResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.fillSameOutfitDyeCode",
      handle_infinity_nikki_fill_same_outfit_dye_code,
      "Fill dye code records on assets with the same Infinity Nikki outfit and dye data, "
      "overwriting existing values");

  core::rpc::register_method<::extensions::infinity_nikki::SetInfinityNikkiWorldRecordParams,
                             features::gallery::OperationResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.setWorldRecord",
      handle_infinity_nikki_set_world_record,
      "Set or clear a single Infinity Nikki world record in the gallery details panel");

  Logger().info("Extensions RPC endpoints registered");
}

}  // namespace core::rpc::endpoints::extensions
