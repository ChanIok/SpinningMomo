module;

export module Core.Events.Registrar;

import Core.State;

namespace Core::Events {

export auto register_all_handlers(Core::State::AppState& app_state) -> void;

}  // namespace Core::Events