module;

module Core.Events.Registrar;

import Core.State;
import Core.Events.Handlers.Feature;
import Core.Events.Handlers.Settings;
import Core.Events.Handlers.System;

namespace Core::Events {

auto register_all_handlers(Core::State::AppState& app_state) -> void {
  Handlers::register_feature_handlers(app_state);
  Handlers::register_settings_handlers(app_state);
  Handlers::register_system_handlers(app_state);
}

}  // namespace Core::Events