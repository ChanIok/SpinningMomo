#pragma once

#include "core/state/app_state.hpp"
#include "features/settings/types.hpp"

namespace Features::Settings::Background {

auto analyze_background(const Types::BackgroundAnalysisParams& params)
    -> std::expected<Types::BackgroundAnalysisResult, std::string>;

auto import_background_image(const Types::BackgroundImportParams& params)
    -> std::expected<Types::BackgroundImportResult, std::string>;

auto remove_background_image(const Types::BackgroundRemoveParams& params)
    -> std::expected<Types::BackgroundRemoveResult, std::string>;

auto register_static_resolvers(Core::State::AppState& app_state) -> void;

}  // namespace Features::Settings::Background
