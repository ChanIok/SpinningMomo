module;

#include <asio.hpp>

module Core.RPC.Endpoints.Extensions;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Core.RPC.NotificationHub;
import Extensions.InfinityNikki.AssetService;
import Extensions.InfinityNikki.TaskService;
import Extensions.InfinityNikki.Types;
import Extensions.InfinityNikki.GameDirectory;
import Features.Gallery.Types;
import Utils.Logger;
import <rfl/json.hpp>;

namespace Core::RPC::Endpoints::Extensions {

struct StartExtensionTaskResult {
  std::string task_id;
};

auto handle_infinity_nikki_get_game_directory([[maybe_unused]] Core::State::AppState& app_state,
                                              [[maybe_unused]] const rfl::Generic& params)
    -> Core::RPC::RpcAwaitable<::Extensions::InfinityNikki::InfinityNikkiGameDirResult> {
  auto result = ::Extensions::InfinityNikki::GameDirectory::get_game_directory();
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Failed to get Infinity Nikki game directory: " + result.error(),
    });
  }

  co_return result.value();
}

auto handle_infinity_nikki_start_extract_photo_params(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& params)
    -> Core::RPC::RpcAwaitable<StartExtensionTaskResult> {
  auto task_result =
      ::Extensions::InfinityNikki::TaskService::start_extract_photo_params_task(app_state, params);
  if (!task_result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::InvalidRequest),
        .message = task_result.error(),
    });
  }
  co_return StartExtensionTaskResult{.task_id = task_result.value()};
}

auto handle_infinity_nikki_start_extract_photo_params_for_folder(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsForFolderRequest& params)
    -> Core::RPC::RpcAwaitable<StartExtensionTaskResult> {
  auto task_result =
      ::Extensions::InfinityNikki::TaskService::start_extract_photo_params_for_folder_task(
          app_state, params);
  if (!task_result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::InvalidRequest),
        .message = task_result.error(),
    });
  }
  co_return StartExtensionTaskResult{.task_id = task_result.value()};
}

auto handle_infinity_nikki_start_initialize_media_hardlinks(
    Core::State::AppState& app_state, [[maybe_unused]] const rfl::Generic& params)
    -> Core::RPC::RpcAwaitable<StartExtensionTaskResult> {
  auto task_result =
      ::Extensions::InfinityNikki::TaskService::start_initialize_media_hardlinks_task(app_state);
  if (!task_result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::InvalidRequest),
        .message = task_result.error(),
    });
  }
  co_return StartExtensionTaskResult{.task_id = task_result.value()};
}

auto handle_infinity_nikki_query_photo_map_points(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::QueryPhotoMapPointsParams& params)
    -> Core::RPC::RpcAwaitable<std::vector<::Extensions::InfinityNikki::PhotoMapPoint>> {
  auto result =
      co_await ::Extensions::InfinityNikki::AssetService::query_photo_map_points(app_state, params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_get_details(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::GetInfinityNikkiDetailsParams& params)
    -> Core::RPC::RpcAwaitable<::Extensions::InfinityNikki::InfinityNikkiDetails> {
  auto result = co_await ::Extensions::InfinityNikki::AssetService::get_details(app_state, params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_get_map_config(Core::State::AppState& app_state,
                                          [[maybe_unused]] const rfl::Generic& params)
    -> Core::RPC::RpcAwaitable<::Extensions::InfinityNikki::InfinityNikkiMapConfig> {
  auto result = co_await ::Extensions::InfinityNikki::AssetService::get_map_config(app_state);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_get_metadata_names(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::GetInfinityNikkiMetadataNamesParams& params)
    -> Core::RPC::RpcAwaitable<::Extensions::InfinityNikki::InfinityNikkiMetadataNames> {
  auto result =
      co_await ::Extensions::InfinityNikki::AssetService::get_metadata_names(app_state, params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_set_user_record(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::SetInfinityNikkiUserRecordParams& params)
    -> Core::RPC::RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = ::Extensions::InfinityNikki::AssetService::set_user_record(app_state, params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }

  if (result->affected_count.value_or(0) > 0) {
    Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_infinity_nikki_preview_same_outfit_dye_code_fill(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::PreviewInfinityNikkiSameOutfitDyeCodeFillParams& params)
    -> Core::RPC::RpcAwaitable<
        ::Extensions::InfinityNikki::InfinityNikkiSameOutfitDyeCodeFillPreview> {
  auto result = ::Extensions::InfinityNikki::AssetService::preview_same_outfit_dye_code_fill(
      app_state, params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }
  co_return result.value();
}

auto handle_infinity_nikki_fill_same_outfit_dye_code(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::FillInfinityNikkiSameOutfitDyeCodeParams& params)
    -> Core::RPC::RpcAwaitable<
        ::Extensions::InfinityNikki::InfinityNikkiSameOutfitDyeCodeFillResult> {
  auto result =
      ::Extensions::InfinityNikki::AssetService::fill_same_outfit_dye_code(app_state, params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }

  if (result->affected_count > 0) {
    Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto handle_infinity_nikki_set_world_record(
    Core::State::AppState& app_state,
    const ::Extensions::InfinityNikki::SetInfinityNikkiWorldRecordParams& params)
    -> Core::RPC::RpcAwaitable<Features::Gallery::Types::OperationResult> {
  auto result = ::Extensions::InfinityNikki::AssetService::set_world_record(app_state, params);
  if (!result) {
    co_return std::unexpected(Core::RPC::RpcError{
        .code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
        .message = "Service error: " + result.error(),
    });
  }

  if (result->affected_count.value_or(0) > 0) {
    Core::RPC::NotificationHub::send_notification(app_state, "gallery.changed");
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<rfl::Generic, ::Extensions::InfinityNikki::InfinityNikkiGameDirResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getGameDirectory",
      handle_infinity_nikki_get_game_directory,
      "Get Infinity Nikki game installation directory from launcher config");

  Core::RPC::register_method<::Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest,
                             StartExtensionTaskResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.startExtractPhotoParams",
      handle_infinity_nikki_start_extract_photo_params,
      "Create a background task to extract and index Infinity Nikki photo params");

  Core::RPC::register_method<
      ::Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsForFolderRequest,
      StartExtensionTaskResult>(
      app_state, app_state.rpc->registry,
      "extensions.infinityNikki.startExtractPhotoParamsForFolder",
      handle_infinity_nikki_start_extract_photo_params_for_folder,
      "Create a background task to extract Infinity Nikki photo params for a gallery folder");

  Core::RPC::register_method<rfl::Generic, StartExtensionTaskResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.startInitializeMediaHardlinks",
      handle_infinity_nikki_start_initialize_media_hardlinks,
      "Create a background task to initialize Infinity Nikki media hardlinks");

  Core::RPC::register_method<::Extensions::InfinityNikki::QueryPhotoMapPointsParams,
                             std::vector<::Extensions::InfinityNikki::PhotoMapPoint>>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.queryPhotoMapPoints",
      handle_infinity_nikki_query_photo_map_points,
      "Query Infinity Nikki photo map points using the current gallery filters");

  Core::RPC::register_method<::Extensions::InfinityNikki::GetInfinityNikkiDetailsParams,
                             ::Extensions::InfinityNikki::InfinityNikkiDetails>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getDetails",
      handle_infinity_nikki_get_details,
      "Get Infinity Nikki extracted data and user record for the specified asset");

  Core::RPC::register_method<rfl::Generic, ::Extensions::InfinityNikki::InfinityNikkiMapConfig>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getMapConfig",
      handle_infinity_nikki_get_map_config, "Get online Infinity Nikki map world configuration");

  Core::RPC::register_method<::Extensions::InfinityNikki::GetInfinityNikkiMetadataNamesParams,
                             ::Extensions::InfinityNikki::InfinityNikkiMetadataNames>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.getMetadataNames",
      handle_infinity_nikki_get_metadata_names,
      "Resolve localized names for Infinity Nikki metadata ids such as pose/filter/light");

  Core::RPC::register_method<::Extensions::InfinityNikki::SetInfinityNikkiUserRecordParams,
                             Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.setUserRecord",
      handle_infinity_nikki_set_user_record,
      "Set or clear a single Infinity Nikki user record in the gallery details panel");

  Core::RPC::register_method<
      ::Extensions::InfinityNikki::PreviewInfinityNikkiSameOutfitDyeCodeFillParams,
      ::Extensions::InfinityNikki::InfinityNikkiSameOutfitDyeCodeFillPreview>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.previewSameOutfitDyeCodeFill",
      handle_infinity_nikki_preview_same_outfit_dye_code_fill,
      "Preview how many same Infinity Nikki outfit and dye assets can receive the current dye "
      "code");

  Core::RPC::register_method<::Extensions::InfinityNikki::FillInfinityNikkiSameOutfitDyeCodeParams,
                             ::Extensions::InfinityNikki::InfinityNikkiSameOutfitDyeCodeFillResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.fillSameOutfitDyeCode",
      handle_infinity_nikki_fill_same_outfit_dye_code,
      "Fill dye code records on assets with the same Infinity Nikki outfit and dye data, "
      "overwriting existing values");

  Core::RPC::register_method<::Extensions::InfinityNikki::SetInfinityNikkiWorldRecordParams,
                             Features::Gallery::Types::OperationResult>(
      app_state, app_state.rpc->registry, "extensions.infinityNikki.setWorldRecord",
      handle_infinity_nikki_set_world_record,
      "Set or clear a single Infinity Nikki world record in the gallery details panel");

  Logger().info("Extensions RPC endpoints registered");
}

}  // namespace Core::RPC::Endpoints::Extensions
