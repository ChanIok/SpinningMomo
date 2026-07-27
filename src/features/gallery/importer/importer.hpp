#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::importer {

// 将外部普通媒体文件复制到指定图库文件夹，并同步建立索引。
auto import_files_to_folder(core::AppState& app_state, std::int64_t folder_id,
                            const std::vector<std::filesystem::path>& source_paths)
    -> std::expected<OperationResult, std::string>;

}  // namespace features::gallery::importer
