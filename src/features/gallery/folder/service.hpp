#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::folder::service {

struct BatchCreateFoldersResult {
  std::unordered_map<std::string, std::int64_t> folder_ids_by_path;
  std::vector<Folder> created_folders;
};

// 构建文件夹层次结构信息
auto build_folder_hierarchy(const std::vector<std::filesystem::path>& paths)
    -> std::vector<FolderHierarchy>;

// 从路径集合中提取所有唯一的文件夹路径（包含所有祖先目录直到扫描根目录）
auto extract_unique_folder_paths(const std::vector<std::filesystem::path>& file_paths,
                                 const std::filesystem::path& scan_root)
    -> std::vector<std::filesystem::path>;

// 按父先优先顺序物化目录，并同时返回完整路径映射和本轮新增目录。
auto batch_create_folders_for_paths(core::AppState& app_state,
                                    const std::vector<std::filesystem::path>& folder_paths)
    -> std::expected<BatchCreateFoldersResult, std::string>;

// 复用调用方已经加载的局部目录库存，在一个事务中物化缺失目录。
auto batch_create_folders_for_paths(core::AppState& app_state,
                                    const std::vector<std::filesystem::path>& folder_paths,
                                    const std::vector<Folder>& folder_inventory)
    -> std::expected<BatchCreateFoldersResult, std::string>;

// 根据数据库里的根文件夹记录，确保 WebView 原图 host mappings 全部就绪。
auto ensure_all_root_folder_webview_mappings(core::AppState& app_state)
    -> std::expected<void, std::string>;

// 在已索引父目录下创建真实子目录，并立即物化对应文件夹记录。
auto create_child_folder(core::AppState& app_state, std::int64_t parent_folder_id,
                         const std::string& name) -> std::expected<OperationResult, std::string>;

// 更新文件夹显示名称（仅应用内展示）。
auto update_folder_display_name(core::AppState& app_state, std::int64_t folder_id,
                                const std::optional<std::string>& display_name)
    -> std::expected<OperationResult, std::string>;

// 仅在文件夹尚无显示名称时写入名称，并返回本次是否实际更新。
auto update_folder_display_name_if_empty(core::AppState& app_state, std::int64_t folder_id,
                                         const std::string& display_name)
    -> std::expected<bool, std::string>;

// 在系统资源管理器中打开文件夹。
auto open_folder_in_explorer(core::AppState& app_state, std::int64_t folder_id)
    -> std::expected<OperationResult, std::string>;

// 移除根文件夹监听并清理对应索引（包含子文件夹）。
auto remove_root_folder_watch(core::AppState& app_state, std::int64_t folder_id)
    -> std::expected<OperationResult, std::string>;

}  // namespace features::gallery::folder::service
