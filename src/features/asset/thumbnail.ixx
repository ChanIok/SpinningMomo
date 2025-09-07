module;

export module Features.Asset.Thumbnail;

import std;
import Core.State;
import Features.Asset.Types;
import Utils.Image;

export namespace Features::Asset::Thumbnail {

// ============= 缩略图生成功能 =============

// 为单个资产项生成缩略图
auto generate_thumbnail_for_asset(Core::State::AppState& app_state,
                                  Utils::Image::WICFactory& wic_factory, const Types::Asset& asset,
                                  std::uint32_t max_width = 400, std::uint32_t max_height = 400,
                                  const Utils::Image::WebPEncodeOptions& options = {})
    -> std::expected<std::filesystem::path, std::string>;

// 批量生成缩略图
auto batch_generate_thumbnails(Core::State::AppState& app_state,
                               Utils::Image::WICFactory& wic_factory,
                               const std::vector<Types::Asset>& assets,
                               std::uint32_t max_width = 400, std::uint32_t max_height = 400)
    -> std::expected<std::vector<std::filesystem::path>, std::string>;

// ============= 缩略图路径管理 =============

// 确保缩略图目录存在
auto ensure_thumbnails_directory_exists(Core::State::AppState& app_state) -> std::expected<void, std::string>;

// 根据资产项ID生成缩略图文件名
auto generate_thumbnail_filename(std::int64_t asset_id, const std::string& original_filename)
    -> std::string;

// 根据资产项获取缩略图完整路径
auto get_thumbnail_path(Core::State::AppState& app_state, const Types::Asset& asset)
    -> std::expected<std::filesystem::path, std::string>;

// ============= 缩略图清理功能 =============

// 清理无效的缩略图文件（对应的资产项已被删除）
auto cleanup_orphaned_thumbnails(Core::State::AppState& app_state)
    -> std::expected<int, std::string>;

// 删除指定资产项的缩略图
auto delete_thumbnail(Core::State::AppState& app_state, const Types::Asset& asset) -> std::expected<void, std::string>;

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

}  // namespace Features::Asset::Thumbnail
