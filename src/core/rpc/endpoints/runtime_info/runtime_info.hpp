#pragma once

#include "core/state/app_state.hpp"

namespace Core::RPC::Endpoints::RuntimeInfo {

auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::RuntimeInfo
