module;

export module Core.Events.Handlers.System;

import Core.State;

namespace Core::Events::Handlers {

export auto register_system_handlers(Core::State::AppState& app_state) -> void;

}