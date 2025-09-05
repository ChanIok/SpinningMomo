module;

export module Features.Media.Thumbnail;

import std;
import Core.State;
import Features.Media.Types;
import Utils.Image;

export namespace Features::Media::Thumbnail {

// ============= 缩略图生成功能 =============

// 为单个媒体项生成缩略图
auto generate_thumbnail_for_media(Core::State::AppState& app_state,
                                 Utils::Image::WICFactory& wic_factory,
                                 const Types::MediaItem& media_item,
                                 std::uint32_t max_width = 400,
                                 std::uint32_t max_height = 400,
                                 const Utils::Image::WebPEncodeOptions& options = {})
    -> std::expected<std::filesystem::path, std::string>;

// 批量生成缩略图
auto batch_generate_thumbnails(Core::State::AppState& app_state,
                              Utils::Image::WICFactory& wic_factory,
                              const std::vector<Types::MediaItem>& media_items,
                              std::uint32_t max_width = 400,
                              std::uint32_t max_height = 400)
    -> std::expected<std::vector<std::filesystem::path>, std::string>;

// ============= 缩略图路径管理 =============

// 获取缩略图存储目录
auto get_thumbnails_directory() -> std::expected<std::filesystem::path, std::string>;

// 根据媒体项ID生成缩略图文件名
auto generate_thumbnail_filename(std::int64_t media_id, const std::string& original_filename)
    -> std::string;

// 根据媒体项获取缩略图完整路径
auto get_thumbnail_path(const Types::MediaItem& media_item)
    -> std::expected<std::filesystem::path, std::string>;

// 确保缩略图目录存在
auto ensure_thumbnails_directory_exists() -> std::expected<void, std::string>;

// ============= 缩略图访问功能 =============

// 获取缩略图数据（用于 RPC 返回）
auto get_thumbnail_data(Core::State::AppState& app_state, std::int64_t media_id)
    -> std::expected<std::vector<std::uint8_t>, std::string>;

// 检查缩略图是否存在
auto thumbnail_exists(const std::filesystem::path& thumbnail_path) -> bool;

// 获取缩略图文件信息
auto get_thumbnail_info(const std::filesystem::path& thumbnail_path)
    -> std::expected<std::tuple<std::uint32_t, std::uint32_t, std::int64_t>, std::string>; // width, height, file_size

// ============= 缩略图清理功能 =============

// 清理无效的缩略图文件（对应的媒体项已被删除）
auto cleanup_orphaned_thumbnails(Core::State::AppState& app_state)
    -> std::expected<int, std::string>;

// 删除指定媒体项的缩略图
auto delete_thumbnail(const Types::MediaItem& media_item)
    -> std::expected<void, std::string>;

// 批量删除缩略图
auto batch_delete_thumbnails(const std::vector<Types::MediaItem>& media_items)
    -> std::expected<int, std::string>;

// ============= 缩略图验证功能 =============

// 验证缩略图是否需要重新生成（基于原文件修改时间）
auto needs_thumbnail_regeneration(const Types::MediaItem& media_item)
    -> std::expected<bool, std::string>;

// 验证缩略图文件完整性
auto validate_thumbnail_integrity(const std::filesystem::path& thumbnail_path)
    -> std::expected<bool, std::string>;

// ============= 缩略图统计功能 =============

// 缩略图统计信息
struct ThumbnailStats {
    int total_thumbnails;
    std::int64_t total_size;
    int orphaned_thumbnails;
    int corrupted_thumbnails;
    std::string thumbnails_directory;
};

// 获取缩略图统计信息
auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<ThumbnailStats, std::string>;

// ============= 高级功能 =============

// 异步生成缩略图（预留接口）
struct ThumbnailGenerationTask {
    std::int64_t media_id;
    std::filesystem::path source_path;
    std::filesystem::path thumbnail_path;
    std::uint32_t max_width;
    std::uint32_t max_height;
    Utils::Image::WebPEncodeOptions options;
};

// 生成缩略图的结果
struct ThumbnailGenerationResult {
    bool success;
    std::filesystem::path thumbnail_path;
    std::string error_message;
    std::uint32_t generated_width;
    std::uint32_t generated_height;
    std::int64_t file_size;
};

// 执行单个缩略图生成任务
auto execute_thumbnail_task(Utils::Image::WICFactory& wic_factory,
                           const ThumbnailGenerationTask& task)
    -> ThumbnailGenerationResult;

} // namespace Features::Media::Thumbnail
