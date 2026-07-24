#pragma once

#include "extensions/infinity_nikki/types.hpp"

namespace Extensions::InfinityNikki::GameDirectory {

auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string>;

}  // namespace Extensions::InfinityNikki::GameDirectory
