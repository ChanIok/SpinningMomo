module;

export module Features.Gallery.Folder;

import std;
import Core.State;
import Features.Gallery.Types;

export namespace Features::Gallery::Folder {

// ============= 文件夹管理功能 =============

// 获取或创建文件夹记录
auto get_or_create_folder(Core::State::AppState& app_state, const std::string& folder_path)
    -> std::expected<std::int64_t, std::string>;

// 更新文件夹资产数量
auto update_folder_asset_count(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<void, std::string>;

// 设置文件夹封面
auto set_folder_cover(Core::State::AppState& app_state, std::int64_t folder_id, 
                      std::int64_t cover_asset_id)
    -> std::expected<void, std::string>;

// ============= 文件夹查询功能 =============

// 获取文件夹树结构
auto get_folder_tree(Core::State::AppState& app_state)
    -> std::expected<std::vector<std::string>, std::string>;

// 获取文件夹信息
auto get_folder_by_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::optional<std::string>, std::string>;

}  // namespace Features::Gallery::Folder
