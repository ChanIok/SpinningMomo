module;

module Core.RPC.Endpoints.Registry;

import std;
import Core.State;
import Core.RPC;
import Core.RPC.State;
import Core.RPC.Types;
import Features.Registry;
import <asio.hpp>;

namespace Core::RPC::Endpoints::Registry {

auto handle_get_all_features(Core::State::AppState& app_state,
                             const Features::Registry::GetAllFeaturesParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Features::Registry::GetAllFeaturesResult>> {
  try {
    if (!app_state.feature_registry) {
      co_return std::unexpected(
          Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                              .message = "Feature registry not initialized"});
    }

    // 获取所有功能描述符
    auto all_features = Features::Registry::get_all_features(*app_state.feature_registry);

    // 转换为 RPC 传输格式
    Features::Registry::GetAllFeaturesResult result;
    result.features.reserve(all_features.size());

    for (const auto& feature : all_features) {
      Features::Registry::FeatureDescriptorData data{
          .id = feature.id,
          .i18n_key = feature.i18n_key,
          .is_toggle = feature.is_toggle,
      };
      result.features.push_back(std::move(data));
    }

    co_return result;
  } catch (const std::exception& e) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to get features: " + std::string(e.what())});
  }
}

auto register_all(Core::State::AppState& app_state) -> void {
  Core::RPC::register_method<Features::Registry::GetAllFeaturesParams,
                             Features::Registry::GetAllFeaturesResult>(
      app_state, app_state.rpc->registry, "features.getAll", handle_get_all_features,
      "Get all available feature descriptors");
}

}  // namespace Core::RPC::Endpoints::Registry
