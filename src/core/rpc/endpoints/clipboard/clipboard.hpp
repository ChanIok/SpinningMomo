#pragma once

#include "core/state/app_state.hpp"

namespace Core::RPC::Endpoints::Clipboard {

auto register_all(Core::State::AppState& state) -> void;

}  // namespace Core::RPC::Endpoints::Clipboard
