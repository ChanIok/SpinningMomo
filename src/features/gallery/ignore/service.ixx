module;

export module Features.Gallery.Ignore.Service;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Ignore::Service {

// 沿文件夹层级解析忽略规则所属的顶层监听目录
export auto resolve_root_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::int64_t, std::string>;

// 加载并合并忽略规则（先加载全局规则，再加载当前目录所属 root 文件夹的规则）
export auto load_ignore_rules(Core::State::AppState& app_state,
                              std::optional<std::int64_t> folder_id = std::nullopt)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

// 按文件或目录语义应用忽略规则，返回该路径是否应被排除。
export auto apply_ignore_rules(const std::filesystem::path& path,
                               const std::filesystem::path& base_path,
                               const std::vector<Types::IgnoreRule>& rules, bool is_directory)
    -> bool;

}  // namespace Features::Gallery::Ignore::Service
