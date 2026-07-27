#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace extensions::infinity_nikki::role_profile {

// 在新建目录是直属 UID 文件夹时异步补全其角色昵称。
auto schedule_nickname_sync(core::AppState& app_state,
                            const std::filesystem::path& game_play_photos_root,
                            const features::gallery::Folder& folder) -> void;

}  // namespace extensions::infinity_nikki::role_profile
