module;

export module Core.RPC.Endpoints.Registry;

import Core.State;

namespace Core::RPC::Endpoints::Registry {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Registry
