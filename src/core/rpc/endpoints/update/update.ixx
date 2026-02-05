module;

export module Core.RPC.Endpoints.Update;

import Core.State;

namespace Core::RPC::Endpoints::Update {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Update
