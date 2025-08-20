module;

export module Core.RPC.Endpoints.Settings;

import Core.State;

namespace Core::RPC::Endpoints::Settings {

export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Settings