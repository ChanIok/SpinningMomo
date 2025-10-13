module;

export module Features.Gallery.Folder.Processor;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Folder::Processor {

// 构建文件夹层次结构信息
export auto build_folder_hierarchy(const std::vector<std::filesystem::path>& paths)
    -> std::vector<Types::FolderHierarchy>;

// 从路径集合中提取所有唯一的文件夹路径（包含所有祖先目录直到扫描根目录）
export auto extract_unique_folder_paths(const std::vector<std::filesystem::path>& file_paths,
                                        const std::filesystem::path& scan_root)
    -> std::vector<std::filesystem::path>;

// 批量创建文件夹记录（统一的文件夹创建接口）
export auto batch_create_folders_for_paths(Core::State::AppState& app_state,
                                           const std::vector<std::filesystem::path>& folder_paths)
    -> std::expected<std::unordered_map<std::string, std::int64_t>, std::string>;


}  // namespace Features::Gallery::Folder::Processor
