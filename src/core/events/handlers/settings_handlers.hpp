#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"

namespace core::events::handlers {

auto register_settings_handlers(core::AppState& app_state) -> void;

}
