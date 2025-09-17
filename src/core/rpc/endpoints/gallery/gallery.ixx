module;

export module Core.RPC.Endpoints.Gallery;

import Core.State;

namespace Core::RPC::Endpoints::Gallery {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Gallery
