#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/settings/types.hpp"

namespace features::settings::background {

auto analyze_background(const BackgroundAnalysisParams& params)
    -> std::expected<BackgroundAnalysisResult, std::string>;

auto import_background_image(const BackgroundImportParams& params)
    -> std::expected<BackgroundImportResult, std::string>;

auto remove_background_image(const BackgroundRemoveParams& params)
    -> std::expected<BackgroundRemoveResult, std::string>;

auto register_static_resolvers(core::AppState& app_state) -> void;

}  // namespace features::settings::background
