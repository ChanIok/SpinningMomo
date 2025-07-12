module;

module Handlers.EventRegistrar;

import Core.State;
import Handlers.Feature;
import Handlers.System;
import Handlers.Window;
import Handlers.WebView;

namespace Handlers {

auto register_all_handlers(Core::State::AppState& app_state) -> void {
  register_feature_handlers(app_state);
  register_system_handlers(app_state);
  register_window_handlers(app_state);
  register_webview_handlers(app_state);
}

} 