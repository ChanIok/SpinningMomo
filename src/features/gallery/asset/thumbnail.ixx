module;

export module Features.Gallery.Asset.Thumbnail;

import std;
import Core.State;
import Features.Gallery.Types;
import Utils.Image;

export namespace Features::Gallery::Asset::Thumbnail {

// 仅用于“补缺失缩略图”场景的统计。
// 这里不关心孤儿缩略图，因为局部修复不会删除它们。
struct ThumbnailRepairStats {
  // 去重后的候选 hash 数；不是资产条数。
  int candidate_hashes = 0;
  // 期望存在但当前磁盘上不存在的缩略图数。
  int missing_thumbnails = 0;
  // 本次实际补回成功的缩略图数。
  int repaired_thumbnails = 0;
  // 生成/写入失败的次数。
  int failed_repairs = 0;
  // 期望补图，但找不到任何可用原图源文件的次数。
  int skipped_missing_sources = 0;
};

// 用于“全局缓存对账”场景的统计。
// 启动时会同时关注：缺失缩略图补回 + 孤儿缩略图清理。
struct ThumbnailCacheReconcileStats {
  // DB 中“理论上应当存在缩略图”的去重 hash 总数。
  int expected_hashes = 0;
  // 缩略图目录里实际扫描到的 .webp 文件数（按 hash 去重后）。
  int existing_thumbnails = 0;
  // expected - existing 的数量。
  int missing_thumbnails = 0;
  // 缺失缩略图中，最终成功补回的数量。
  int repaired_thumbnails = 0;
  // existing - expected 的数量。
  int orphaned_thumbnails = 0;
  // 孤儿缩略图中，实际删除成功的数量。
  int deleted_orphaned_thumbnails = 0;
  // 补图失败次数。
  int failed_repairs = 0;
  // 删除孤儿缩略图失败次数。
  int failed_orphan_deletions = 0;
  // 理论缺失，但没有任何可用源文件可用于重建的次数。
  int skipped_missing_sources = 0;
};

// 缩略图生成
auto generate_thumbnail(Core::State::AppState& app_state, Utils::Image::WICFactory& wic_factory,
                        const std::filesystem::path& source_file, const std::string& file_hash,
                        std::uint32_t short_edge_size)
    -> std::expected<std::filesystem::path, std::string>;

// 落盘内存中的 WebP（视频封面帧等）；路径规则与 generate_thumbnail 一致。
auto save_thumbnail_data(Core::State::AppState& app_state, const std::string& file_hash,
                         const Utils::Image::WebPEncodedResult& webp_data)
    -> std::expected<std::filesystem::path, std::string>;

// 路径管理
auto ensure_thumbnails_directory_exists(Core::State::AppState& app_state)
    -> std::expected<void, std::string>;

auto ensure_thumbnail_path(Core::State::AppState& app_state, const std::string& file_hash)
    -> std::expected<std::filesystem::path, std::string>;

auto repair_missing_thumbnails(Core::State::AppState& app_state,
                               std::optional<std::filesystem::path> root_directory = std::nullopt,
                               std::uint32_t short_edge_size = 480)
    -> std::expected<ThumbnailRepairStats, std::string>;

// 启动后的全局缓存对账：
// 1. 用 DB 推导“应存在的缩略图集合”
// 2. 用磁盘枚举“实际存在的缩略图集合”
// 3. 补 missing，删 orphan
auto reconcile_thumbnail_cache(Core::State::AppState& app_state,
                               std::uint32_t short_edge_size = 480)
    -> std::expected<ThumbnailCacheReconcileStats, std::string>;

// 清理功能
auto cleanup_orphaned_thumbnails(Core::State::AppState& app_state)
    -> std::expected<int, std::string>;

auto delete_thumbnail(Core::State::AppState& app_state, const Types::Asset& asset)
    -> std::expected<void, std::string>;

// 统计信息
struct AssetThumbnailStats {
  int total_thumbnails;
  std::int64_t total_size;
  int orphaned_thumbnails;
  int corrupted_thumbnails;
  std::string thumbnails_directory;
};

auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<AssetThumbnailStats, std::string>;

}  // namespace Features::Gallery::Asset::Thumbnail
