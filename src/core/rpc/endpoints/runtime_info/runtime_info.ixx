module;

export module Core.RPC.Endpoints.RuntimeInfo;

import Core.State;

namespace Core::RPC::Endpoints::RuntimeInfo {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::RuntimeInfo
