module;

module Handlers.EventRegistrar;

import Core.State;
import Handlers.Feature;
import Handlers.System;
import Handlers.UI;

namespace Handlers {
auto register_all_handlers(Core::State::AppState& app_state) -> void {
  register_feature_handlers(app_state);
  register_system_handlers(app_state);
  register_ui_handlers(app_state);
}
} 