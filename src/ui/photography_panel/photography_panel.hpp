#pragma once

#include "core/state/app_state.hpp"

namespace UI::PhotographyPanel {

auto show(Core::State::AppState& state) -> std::expected<void, std::string>;
auto hide(Core::State::AppState& state) -> void;
auto request_repaint(Core::State::AppState& state) -> void;
auto refresh_from_settings(Core::State::AppState& state) -> void;
auto cleanup(Core::State::AppState& state) -> void;

}  // namespace UI::PhotographyPanel
