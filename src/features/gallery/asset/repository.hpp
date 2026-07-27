#pragma once

#include "vendor/std.hpp"

#include "core/database/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::asset::repository {

// 基本操作
auto create_asset(core::AppState& app_state, const Asset& item)
    -> std::expected<std::int64_t, std::string>;

// 调用方已开启事务：创建资产，并从同 hash 最早的既有资产一次性继承用户数据。
auto create_asset_with_inherited_data_in_transaction(core::AppState& app_state, const Asset& item)
    -> std::expected<std::int64_t, std::string>;

auto get_asset_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Asset>, std::string>;

auto get_asset_by_path(core::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Asset>, std::string>;

auto has_assets_under_path_prefix(core::AppState& app_state, const std::string& path_prefix)
    -> std::expected<bool, std::string>;

// 只更新 scanner 从文件系统派生的索引字段，不触碰 description/rating/review_flag。
auto update_asset_scanner_fields(core::AppState& app_state, const Asset& item)
    -> std::expected<void, std::string>;

// 手动移动文件后只更新位置字段，不重写媒体元数据或用户字段。
auto update_asset_location(core::AppState& app_state, std::int64_t asset_id,
                           const std::string& name, const std::string& path,
                           std::optional<std::int64_t> folder_id)
    -> std::expected<void, std::string>;

// 同步内容未变资产的文件状态，避免后续扫描重复计算指纹
auto update_asset_file_state(core::AppState& app_state, std::int64_t asset_id, std::int64_t size,
                             std::int64_t file_modified_at) -> std::expected<void, std::string>;

auto mark_asset_missing_by_path(core::AppState& app_state, const std::string& path)
    -> std::expected<bool, std::string>;

auto restore_assets_by_ids(core::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<void, std::string>;

auto restore_asset_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<bool, std::string>;

auto purge_expired_missing_assets(core::AppState& app_state, std::int64_t cutoff_millis)
    -> std::expected<std::int64_t, std::string>;

auto batch_delete_assets_by_ids(core::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<void, std::string>;

}  // namespace features::gallery::asset::repository
