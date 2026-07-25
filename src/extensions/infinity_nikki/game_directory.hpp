#pragma once

#include "vendor/std.hpp"

#include "extensions/infinity_nikki/types.hpp"

namespace extensions::infinity_nikki::game_directory {

auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string>;

}  // namespace extensions::infinity_nikki::game_directory
