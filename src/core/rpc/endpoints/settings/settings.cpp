module;

module Core.RPC.Endpoints.Settings;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Settings;
import Features.Settings.Types;
import Features.Settings.Background;
import <asio.hpp>;

namespace Core::RPC::Endpoints::Settings {

auto handle_get_settings([[maybe_unused]] Core::State::AppState& app_state,
                         const Features::Settings::Types::GetSettingsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::GetSettingsResult>> {
  auto result = Features::Settings::get_settings(params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_update_settings(Core::State::AppState& app_state,
                            const Features::Settings::Types::UpdateSettingsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::UpdateSettingsResult>> {
  auto result = Features::Settings::update_settings(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_patch_settings(Core::State::AppState& app_state,
                           const Features::Settings::Types::PatchSettingsParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::PatchSettingsResult>> {
  auto result = Features::Settings::patch_settings(app_state, params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_analyze_background([[maybe_unused]] Core::State::AppState& app_state,
                               const Features::Settings::Types::BackgroundAnalysisParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::BackgroundAnalysisResult>> {
  auto result = Features::Settings::Background::analyze_background(params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_import_background([[maybe_unused]] Core::State::AppState& app_state,
                              const Features::Settings::Types::BackgroundImportParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::BackgroundImportResult>> {
  auto result = Features::Settings::Background::import_background_image(params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto handle_remove_background([[maybe_unused]] Core::State::AppState& app_state,
                              const Features::Settings::Types::BackgroundRemoveParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Settings::Types::BackgroundRemoveResult>> {
  auto result = Features::Settings::Background::remove_background_image(params);

  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Service error: " + result.error()});
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<Features::Settings::Types::GetSettingsParams,
                             Features::Settings::Types::GetSettingsResult>(
      app_state, app_state.rpc->registry, "settings.get", handle_get_settings,
      "Get current settings configuration");

  Core::RPC::register_method<Features::Settings::Types::UpdateSettingsParams,
                             Features::Settings::Types::UpdateSettingsResult>(
      app_state, app_state.rpc->registry, "settings.update", handle_update_settings,
      "Update settings configuration");

  Core::RPC::register_method<Features::Settings::Types::PatchSettingsParams,
                             Features::Settings::Types::PatchSettingsResult>(
      app_state, app_state.rpc->registry, "settings.patch", handle_patch_settings,
      "Patch settings configuration");

  Core::RPC::register_method<Features::Settings::Types::BackgroundAnalysisParams,
                             Features::Settings::Types::BackgroundAnalysisResult>(
      app_state, app_state.rpc->registry, "settings.background.analyze", handle_analyze_background,
      "Analyze background image and return recommended theme and overlay colors");

  Core::RPC::register_method<Features::Settings::Types::BackgroundImportParams,
                             Features::Settings::Types::BackgroundImportResult>(
      app_state, app_state.rpc->registry, "settings.background.import", handle_import_background,
      "Import a background image into managed app storage and return its logical path");

  Core::RPC::register_method<Features::Settings::Types::BackgroundRemoveParams,
                             Features::Settings::Types::BackgroundRemoveResult>(
      app_state, app_state.rpc->registry, "settings.background.remove", handle_remove_background,
      "Remove a managed background image from app storage");
}

}  // namespace Core::RPC::Endpoints::Settings
