module;

export module Core.RPC.Endpoints.Media;

import Core.State;

namespace Core::RPC::Endpoints::Media {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Media
