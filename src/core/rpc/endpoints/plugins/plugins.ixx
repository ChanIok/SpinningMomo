module;

export module Core.RPC.Endpoints.PluginEndpoints;

import Core.State;

namespace Core::RPC::Endpoints::PluginEndpoints {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Plugins