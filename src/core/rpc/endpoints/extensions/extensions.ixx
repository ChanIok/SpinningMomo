module;

export module Core.RPC.Endpoints.Extensions;

import Core.State;

namespace Core::RPC::Endpoints::Extensions {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Extensions
