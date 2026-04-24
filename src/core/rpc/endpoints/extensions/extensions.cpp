module;

#include <asio.hpp>

module Core.RPC.Endpoints.Extensions;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Extensions.InfinityNikki.TaskService;
import Extensions.InfinityNikki.Types;
import Extensions.InfinityNikki.GameDirectory;
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

  Logger().info("Extensions RPC endpoints registered");
}

}  // namespace Core::RPC::Endpoints::Extensions
