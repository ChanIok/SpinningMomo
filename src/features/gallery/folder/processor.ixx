module;

export module Features.Gallery.Folder.Processor;

import std;
import Core.State;
import Features.Gallery.Types;

export namespace Features::Gallery::Folder::Processor {

// ============= 文件夹处理功能 =============

// 从文件路径推断并获取/创建对应的文件夹记录
auto get_or_create_folder_for_file_path(Core::State::AppState& app_state,
                                        const std::filesystem::path& file_path)
    -> std::expected<std::int64_t, std::string>;

// 构建文件夹层次结构信息
auto build_folder_hierarchy(const std::vector<std::filesystem::path>& paths)
    -> std::vector<Types::FolderHierarchy>;

// 规范化文件夹路径（处理.和..等）
auto normalize_folder_path(const std::filesystem::path& folder_path) -> std::filesystem::path;

// 从路径集合中提取所有唯一的文件夹路径
auto extract_unique_folder_paths(const std::vector<std::filesystem::path>& file_paths)
    -> std::vector<std::filesystem::path>;

// 批量创建文件夹记录
auto batch_create_folders_for_paths(Core::State::AppState& app_state,
                                    const std::vector<std::filesystem::path>& folder_paths)
    -> std::expected<std::unordered_map<std::string, std::int64_t>, std::string>;

// ============= 资产与文件夹关联功能 =============

// 更新资产的folder_id字段
auto associate_assets_with_folders(Core::State::AppState& app_state,
                                   const std::vector<Types::Asset>& assets)
    -> std::expected<void, std::string>;

// 重新计算所有文件夹的资产计数
auto recalculate_all_folder_asset_counts(Core::State::AppState& app_state)
    -> std::expected<std::vector<std::pair<std::int64_t, int>>, std::string>;  // folder_id, count

}  // namespace Features::Gallery::Folder::Processor
