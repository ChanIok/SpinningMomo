module;

export module Features.Gallery.Scanner;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Scanner {

export auto scan_asset_directory(Core::State::AppState& app_state,
                                 const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string>;

}  // namespace Features::Gallery::Scanner
