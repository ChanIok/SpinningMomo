module;

export module Plugins.InfinityNikki.GameDirectory;

import std;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki::GameDirectory {

export auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string>;

}  // namespace Plugins::InfinityNikki::GameDirectory
