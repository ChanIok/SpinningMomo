module;

export module Features.Settings.BackgroundAnalyzer;

import std;
import Features.Settings.Types;

namespace Features::Settings::BackgroundAnalyzer {

export auto analyze_background(const Types::AnalyzeBackgroundParams& params)
    -> std::expected<Types::AnalyzeBackgroundResult, std::string>;

}  // namespace Features::Settings::BackgroundAnalyzer
