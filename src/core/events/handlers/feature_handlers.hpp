#pragma once

#include "core/state/app_state.hpp"

namespace Core::Events::Handlers {

auto register_feature_handlers(Core::State::AppState& app_state) -> void;

}