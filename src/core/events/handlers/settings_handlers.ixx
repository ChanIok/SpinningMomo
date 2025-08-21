module;

export module Core.Events.Handlers.Settings;

import Core.State;

namespace Core::Events::Handlers {

export auto register_settings_handlers(Core::State::AppState& app_state) -> void;

}