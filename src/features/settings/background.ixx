module;

export module Features.Settings.Background;

import std;
import Core.State;
import Features.Settings.Types;

namespace Features::Settings::Background {

export auto analyze_background(const Types::BackgroundAnalysisParams& params)
    -> std::expected<Types::BackgroundAnalysisResult, std::string>;

export auto import_background_image(const Types::BackgroundImportParams& params)
    -> std::expected<Types::BackgroundImportResult, std::string>;

export auto remove_background_image(const Types::BackgroundRemoveParams& params)
    -> std::expected<Types::BackgroundRemoveResult, std::string>;

export auto register_static_resolvers(Core::State::AppState& app_state) -> void;

}  // namespace Features::Settings::Background
