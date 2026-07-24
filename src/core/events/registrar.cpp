#include "core/events/registrar.hpp"

#include "core/events/handlers/feature_handlers.hpp"
#include "core/events/handlers/settings_handlers.hpp"
#include "core/events/handlers/system_handlers.hpp"
#include "core/state/app_state.hpp"

namespace Core::Events {

auto register_all_handlers(Core::State::AppState& app_state) -> void {
  Handlers::register_feature_handlers(app_state);
  Handlers::register_settings_handlers(app_state);
  Handlers::register_system_handlers(app_state);
}

}  // namespace Core::Events