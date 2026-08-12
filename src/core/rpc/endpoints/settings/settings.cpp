#include "core/rpc/endpoints/settings/settings.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "features/settings/background.hpp"
#include "features/settings/settings.hpp"
#include "features/settings/types.hpp"

namespace core::rpc::endpoints::settings {

auto handle_get_settings(core::AppState& app_state,
                         [[maybe_unused]] const features::settings::GetSettingsParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::settings::GetSettingsResult>> {
  co_return features::settings::get_settings(app_state);
}

auto handle_update_settings(core::AppState& app_state,
                            const features::settings::UpdateSettingsParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::settings::UpdateSettingsResult>> {
  auto result = features::settings::update_settings(app_state, params);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_patch_settings(core::AppState& app_state,
                           const features::settings::PatchSettingsParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::settings::PatchSettingsResult>> {
  auto result = features::settings::patch_settings(app_state, params);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_analyze_background([[maybe_unused]] core::AppState& app_state,
                               const features::settings::BackgroundAnalysisParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::settings::BackgroundAnalysisResult>> {
  auto result = features::settings::background::analyze_background(params);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_import_background([[maybe_unused]] core::AppState& app_state,
                              const features::settings::BackgroundImportParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::settings::BackgroundImportResult>> {
  auto result = features::settings::background::import_background_image(params);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_remove_background([[maybe_unused]] core::AppState& app_state,
                              const features::settings::BackgroundRemoveParams& params)
    -> asio::awaitable<core::rpc::RpcResult<features::settings::BackgroundRemoveResult>> {
  auto result = features::settings::background::remove_background_image(params);

  if (!result) {
    co_return std::unexpected(
        core::rpc::RpcError{.code = static_cast<int>(core::rpc::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto register_all(core::AppState& app_state) -> void {
  // 读取设置只用于页面初始化；修改设置和文件操作接口继续保持 local。
  core::rpc::register_method<features::settings::GetSettingsParams,
                             features::settings::GetSettingsResult>(
      app_state, app_state.rpc->registry, "settings.get", handle_get_settings,
      "Get current settings configuration", core::rpc::AccessLevel::lan);

  core::rpc::register_method<features::settings::UpdateSettingsParams,
                             features::settings::UpdateSettingsResult>(
      app_state, app_state.rpc->registry, "settings.update", handle_update_settings,
      "Update settings configuration");

  core::rpc::register_method<features::settings::PatchSettingsParams,
                             features::settings::PatchSettingsResult>(
      app_state, app_state.rpc->registry, "settings.patch", handle_patch_settings,
      "Patch settings configuration");

  core::rpc::register_method<features::settings::BackgroundAnalysisParams,
                             features::settings::BackgroundAnalysisResult>(
      app_state, app_state.rpc->registry, "settings.background.analyze", handle_analyze_background,
      "Analyze background image and return recommended theme and overlay colors");

  core::rpc::register_method<features::settings::BackgroundImportParams,
                             features::settings::BackgroundImportResult>(
      app_state, app_state.rpc->registry, "settings.background.import", handle_import_background,
      "Import a background image into managed app storage and return its logical path");

  core::rpc::register_method<features::settings::BackgroundRemoveParams,
                             features::settings::BackgroundRemoveResult>(
      app_state, app_state.rpc->registry, "settings.background.remove", handle_remove_background,
      "Remove a managed background image from app storage");
}

}  // namespace core::rpc::endpoints::settings
