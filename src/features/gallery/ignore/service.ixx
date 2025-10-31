module;

export module Features.Gallery.Ignore.Service;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Ignore::Service {

// 加载并合并忽略规则（先加载全局规则，再追加文件夹规则）
export auto load_ignore_rules(Core::State::AppState& app_state,
                              std::optional<std::int64_t> folder_id = std::nullopt)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

// 应用忽略规则到单个文件（返回是否应该忽略）
export auto apply_ignore_rules(const std::filesystem::path& file_path,
                               const std::filesystem::path& base_path,
                               const std::vector<Types::IgnoreRule>& rules) -> bool;

}  // namespace Features::Gallery::Ignore::Service
