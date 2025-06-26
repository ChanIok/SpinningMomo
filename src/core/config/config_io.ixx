module;

export module Core.Config.Io;

import std;
import Core.Config.State;
import Core.Constants;

namespace Core::Config::Io {

export auto initialize() -> std::expected<Core::Config::State::AppConfig, std::string>;

export auto save(const Core::Config::State::AppConfig& config) -> std::expected<void, std::string>;

export auto get_aspect_ratios(const Core::Config::State::AppConfig& config,
                              const Constants::LocalizedStrings& strings)
    -> Core::Config::State::RatioLoadResult;

export auto get_resolution_presets(const Core::Config::State::AppConfig& config,
                                   const Constants::LocalizedStrings& strings)
    -> Core::Config::State::ResolutionLoadResult;

}  // namespace Core::Config::Io