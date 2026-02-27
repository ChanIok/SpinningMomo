module;

export module Features.Gallery.Folder.Service;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Folder::Service {

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

// 更新文件夹显示名称（仅应用内展示）。
export auto update_folder_display_name(Core::State::AppState& app_state, std::int64_t folder_id,
                                       const std::optional<std::string>& display_name)
    -> std::expected<Types::OperationResult, std::string>;

// 在系统资源管理器中打开文件夹。
export auto open_folder_in_explorer(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<Types::OperationResult, std::string>;

// 移除根文件夹监听并清理对应索引（包含子文件夹）。
export auto remove_root_folder_watch(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<Types::OperationResult, std::string>;

}  // namespace Features::Gallery::Folder::Service
