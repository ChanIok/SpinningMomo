module;

export module Core.Config.Io;

import std;
import Types.Config;
import Core.Constants;

namespace Core::Config::Io {

export auto initialize() -> std::expected<Types::Config::AppConfig, std::string>;

export auto save(const Types::Config::AppConfig& config) -> std::expected<void, std::string>;

export auto get_aspect_ratios(const Types::Config::AppConfig& config,
                              const Constants::LocalizedStrings& strings)
    -> Types::Config::RatioLoadResult;

export auto get_resolution_presets(const Types::Config::AppConfig& config,
                                   const Constants::LocalizedStrings& strings)
    -> Types::Config::ResolutionLoadResult;

}  // namespace Core::Config::Io