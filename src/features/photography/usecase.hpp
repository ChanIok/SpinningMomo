#pragma once

#include "core/state/app_state.hpp"

namespace Features::Photography::UseCase {

auto start(Core::State::AppState& state) -> std::expected<void, std::string>;
auto stop(Core::State::AppState& state) -> void;
auto toggle(Core::State::AppState& state) -> void;
auto cleanup(Core::State::AppState& state) -> void;
auto handle_panel_close(Core::State::AppState& state) -> void;

}  // namespace Features::Photography::UseCase
