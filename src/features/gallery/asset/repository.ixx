module;

export module Features.Gallery.Asset.Repository;

import std;
import Core.State;
import Core.Database.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Asset::Repository {

// 基本操作
export auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<std::int64_t, std::string>;

export auto get_asset_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Asset>, std::string>;

export auto get_asset_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Asset>, std::string>;

export auto has_assets_under_path_prefix(Core::State::AppState& app_state,
                                         const std::string& path_prefix)
    -> std::expected<bool, std::string>;

// 只更新 Scanner 从文件系统派生的索引字段，不触碰 description/rating/review_flag。
export auto update_asset_scanner_fields(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<void, std::string>;

// 手动移动文件后只更新位置字段，不重写媒体元数据或用户字段。
export auto update_asset_location(Core::State::AppState& app_state, std::int64_t asset_id,
                                  const std::string& name, const std::string& path,
                                  std::optional<std::int64_t> folder_id)
    -> std::expected<void, std::string>;

// 同步内容未变资产的文件状态，避免后续扫描重复计算指纹
export auto update_asset_file_state(Core::State::AppState& app_state, std::int64_t asset_id,
                                    std::int64_t size, std::int64_t file_modified_at)
    -> std::expected<void, std::string>;

export auto delete_asset(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;
export auto batch_delete_assets_by_ids(Core::State::AppState& app_state,
                                       const std::vector<std::int64_t>& ids)
    -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Asset::Repository
