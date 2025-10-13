module;

export module Features.Gallery.Asset.Thumbnail;

import std;
import Core.State;
import Features.Gallery.Types;
import Utils.Image;

export namespace Features::Gallery::Asset::Thumbnail {

// ============= 缩略图生成功能 =============

// 生成缩略图
auto generate_thumbnail(Core::State::AppState& app_state, Utils::Image::WICFactory& wic_factory,
                        const std::filesystem::path& source_file, const std::string& file_hash,
                        std::uint32_t short_edge_size)
    -> std::expected<std::filesystem::path, std::string>;

// ============= 缩略图路径管理 =============

// 确保缩略图目录存在
auto ensure_thumbnails_directory_exists(Core::State::AppState& app_state)
    -> std::expected<void, std::string>;

// 确保缩略图路径存在（封装路径构建和目录创建）
auto ensure_thumbnail_path(Core::State::AppState& app_state, const std::string& file_hash,
                           uint32_t width, uint32_t height)
    -> std::expected<std::filesystem::path, std::string>;

// ============= 缩略图清理功能 =============

// 清理无效的缩略图文件（对应的资产项已被删除）
auto cleanup_orphaned_thumbnails(Core::State::AppState& app_state)
    -> std::expected<int, std::string>;

// 删除指定资产项的缩略图
auto delete_thumbnail(Core::State::AppState& app_state, const Types::Asset& asset)
    -> std::expected<void, std::string>;

// ============= 缩略图统计功能 =============

// 缩略图统计信息
struct AssetThumbnailStats {
  int total_thumbnails;
  std::int64_t total_size;
  int orphaned_thumbnails;
  int corrupted_thumbnails;
  std::string thumbnails_directory;
};

// 获取缩略图统计信息
auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<AssetThumbnailStats, std::string>;

}  // namespace Features::Gallery::Asset::Thumbnail
