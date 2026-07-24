#pragma once

#include "core/state/app_state.hpp"

namespace Core::Initializer {

auto initialize_application(Core::State::AppState& state) -> std::expected<void, std::string>;

}