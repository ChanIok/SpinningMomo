#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/color/types.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::scanner::asset_pipeline {

// 单路径同步结果：调用方据此统计与组装 ScanChange
enum class PathSyncOutcome {
  Skipped,        // 不支持 / ignore / 未变化 / 库中无且无需删除
  UnchangedMeta,  // 内容指纹未变，仅回写 size/mtime
  Restored,       // 原路径重新出现，恢复原资产行
  Created,
  Updated,
  Missing,  // 盘上无且资产首次进入 missing 宽限期
};

struct PreparedAsset {
  Asset asset;
  std::vector<features::gallery::color::ExtractedColor> colors;
  // true = 库中已有记录（更新），false = 新建
  bool is_update = false;
};

struct MediaPrepareInput {
  std::string hash;
  std::int64_t size = 0;
  std::int64_t file_created_millis = 0;
  std::int64_t file_modified_millis = 0;
  std::optional<std::int64_t> folder_id;
  // 只传更新定位所需的 ID；用户字段不进入 Scanner 的读改写链路。
  std::optional<std::int64_t> existing_asset_id;
};

// 已知指纹与文件状态后：填 Asset + 缩略图/主色，不写库（全量批处理用）
auto prepare_media_asset(core::AppState& app_state, const std::filesystem::path& normalized_path,
                         const ScanOptions& options, const MediaPrepareInput& input)
    -> std::expected<PreparedAsset, std::string>;

// 按路径标记原件缺失；重复事件不重置宽限期，true 表示本次发生状态变化。
auto mark_asset_missing_at_path(core::AppState& app_state, const std::filesystem::path& path)
    -> std::expected<bool, std::string>;

// 增量路径：过滤 → 粗判 → 指纹 → 媒体 → 单条写库
auto upsert_asset_at_path(core::AppState& app_state, const std::filesystem::path& root_path,
                          const ScanOptions& options, const std::vector<IgnoreRule>& ignore_rules,
                          const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                          const std::filesystem::path& path, std::stop_token stop_token)
    -> std::expected<PathSyncOutcome, std::string>;

}  // namespace features::gallery::scanner::asset_pipeline
