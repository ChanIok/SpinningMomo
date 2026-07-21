module;

export module Features.Gallery.Scanner.AssetPipeline;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Color.Types;

namespace Features::Gallery::Scanner::AssetPipeline {

// 单路径同步结果：调用方据此统计与组装 ScanChange
export enum class PathSyncOutcome {
  Skipped,        // 不支持 / ignore / 未变化 / 库中无且无需删除
  UnchangedMeta,  // 内容指纹未变，仅回写 size/mtime
  Created,
  Updated,
  Removed,  // 盘上无且已从库删除（或仅库删除）
};

export struct PreparedAsset {
  Types::Asset asset;
  std::vector<Features::Gallery::Color::Types::ExtractedColor> colors;
  // true = 库中已有记录（更新），false = 新建
  bool is_update = false;
};

export struct MediaPrepareInput {
  std::string hash;
  std::int64_t size = 0;
  std::int64_t file_created_millis = 0;
  std::int64_t file_modified_millis = 0;
  std::optional<std::int64_t> folder_id;
  // 只传更新定位所需的 ID；用户字段不进入 Scanner 的读改写链路。
  std::optional<std::int64_t> existing_asset_id;
};

// 已知指纹与文件状态后：填 Asset + 缩略图/主色，不写库（全量批处理用）
export auto prepare_media_asset(Core::State::AppState& app_state,
                                const std::filesystem::path& normalized_path,
                                const Types::ScanOptions& options, const MediaPrepareInput& input)
    -> std::expected<PreparedAsset, std::string>;

// 按路径删除资产索引；共享缩略图统一由缓存对账回收，true 表示库中确有并已删
export auto remove_asset_at_path(Core::State::AppState& app_state,
                                 const std::filesystem::path& path)
    -> std::expected<bool, std::string>;

// 增量路径：过滤 → 粗判 → 指纹 → 媒体 → 单条写库
export auto upsert_asset_at_path(
    Core::State::AppState& app_state, const std::filesystem::path& root_path,
    const Types::ScanOptions& options, const std::vector<Types::IgnoreRule>& ignore_rules,
    const std::unordered_map<std::string, std::int64_t>& folder_mapping,
    const std::filesystem::path& path, std::stop_token stop_token)
    -> std::expected<PathSyncOutcome, std::string>;

}  // namespace Features::Gallery::Scanner::AssetPipeline
