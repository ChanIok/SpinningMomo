module;

export module Plugins.InfinityNikki;

import std;
import Plugins.InfinityNikki.Types;

namespace Plugins::InfinityNikki {

// 获取游戏目录的核心函数
export auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string>;

}  // namespace Plugins::InfinityNikki