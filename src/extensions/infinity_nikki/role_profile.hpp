#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace extensions::infinity_nikki::role_profile {

// 从本轮新建目录中批量补全直属 UID 文件夹昵称，并在整批结束后合并通知。
auto schedule_nickname_sync_for_created_folders(
    core::AppState& app_state, const std::filesystem::path& game_play_photos_root,
    const std::vector<features::gallery::Folder>& created_folders) -> void;

}  // namespace extensions::infinity_nikki::role_profile
