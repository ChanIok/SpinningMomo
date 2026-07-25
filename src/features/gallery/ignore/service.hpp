#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::ignore::service {

// 沿文件夹层级解析忽略规则所属的顶层监听目录
auto resolve_root_folder_id(core::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::int64_t, std::string>;

// 加载并合并忽略规则（先加载全局规则，再加载当前目录所属 root 文件夹的规则）
auto load_ignore_rules(core::AppState& app_state,
                       std::optional<std::int64_t> folder_id = std::nullopt)
    -> std::expected<std::vector<IgnoreRule>, std::string>;

// 按文件或目录语义应用忽略规则，返回该路径是否应被排除。
auto apply_ignore_rules(const std::filesystem::path& path, const std::filesystem::path& base_path,
                        const std::vector<IgnoreRule>& rules, bool is_directory) -> bool;

}  // namespace features::gallery::ignore::service
