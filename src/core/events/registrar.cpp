#include "core/events/registrar.hpp"

#include "vendor/std.hpp"

#include "core/events/handlers/feature_handlers.hpp"
#include "core/events/handlers/settings_handlers.hpp"
#include "core/events/handlers/system_handlers.hpp"
#include "core/state/app_state.hpp"

namespace core::events {

auto register_all_handlers(core::AppState& app_state) -> void {
  handlers::register_feature_handlers(app_state);
  handlers::register_settings_handlers(app_state);
  handlers::register_system_handlers(app_state);
}

}  // namespace core::events
