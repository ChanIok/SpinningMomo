module;

export module Features.Gallery.Folder.Processor;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Folder::Processor {

// 从文件路径推断并获取/创建对应的文件夹记录
export auto get_or_create_folder_for_file_path(Core::State::AppState& app_state,
                                               const std::filesystem::path& file_path)
    -> std::expected<std::int64_t, std::string>;

// 构建文件夹层次结构信息
export auto build_folder_hierarchy(const std::vector<std::filesystem::path>& paths)
    -> std::vector<Types::FolderHierarchy>;

// 从路径集合中提取所有唯一的文件夹路径
export auto extract_unique_folder_paths(const std::vector<std::filesystem::path>& file_paths)
    -> std::vector<std::filesystem::path>;

// 批量创建文件夹记录
export auto batch_create_folders_for_paths(Core::State::AppState& app_state,
                                           const std::vector<std::filesystem::path>& folder_paths)
    -> std::expected<std::unordered_map<std::string, std::int64_t>, std::string>;


}  // namespace Features::Gallery::Folder::Processor
