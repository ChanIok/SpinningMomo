#pragma once

#include "core/state/app_state.hpp"

namespace Core::RPC::Endpoints::File {

auto register_all(Core::State::AppState& state) -> void;

}  // namespace Core::RPC::Endpoints::File
