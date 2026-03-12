module;

export module Extensions.InfinityNikki.GameDirectory;

import std;
import Extensions.InfinityNikki.Types;

namespace Extensions::InfinityNikki::GameDirectory {

export auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string>;

}  // namespace Extensions::InfinityNikki::GameDirectory
