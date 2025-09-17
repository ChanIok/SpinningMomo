module;

export module Features.Gallery.Ignore.Processor;

import std;
import Core.State;
import Features.Gallery.Types;

export namespace Features::Gallery::Ignore::Processor {

// ============= 基础忽略规则处理函数 =============

// 检查单个文件是否应该被忽略
auto should_ignore_file(const std::filesystem::path& file_path,
                        const std::filesystem::path& base_path, const Types::IgnoreContext& context,
                        std::optional<std::int64_t> folder_id = std::nullopt) -> bool;

// 匹配glob模式（gitignore风格）
auto match_glob_pattern(const std::string& pattern, const std::string& path) -> bool;

// 匹配正则表达式模式
auto match_regex_pattern(const std::string& pattern, const std::string& path) -> bool;

// 应用忽略规则列表到文件路径
auto apply_ignore_rules(const std::filesystem::path& file_path,
                        const std::filesystem::path& base_path,
                        const std::vector<Types::IgnoreRule>& rules) -> bool;

// ============= 辅助函数 =============

// 规范化文件路径用于模式匹配
auto normalize_path_for_matching(const std::filesystem::path& file_path,
                                 const std::filesystem::path& base_path) -> std::string;

}  // namespace Features::Gallery::Ignore::Processor
