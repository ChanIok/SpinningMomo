module;

export module Features.Media.Types;

import std;

export namespace Features::Media::Types {

// 媒体项核心结构
struct MediaItem {
  std::int64_t id;
  std::string filename;
  std::string filepath;       // 完整路径
  std::string relative_path;  // 相对于监控目录的路径
  std::string type;           // "photo", "video", "live_photo", "unknown"

  // 基本信息（来自 Utils::Image::get_image_info）
  std::optional<std::int32_t> width;
  std::optional<std::int32_t> height;
  std::optional<std::int64_t> file_size;
  std::string mime_type;

  // 时间信息
  std::string created_at;                 // 文件创建时间
  std::string updated_at;                 // 数据库更新时间
  std::optional<std::string> deleted_at;  // 软删除

  // 缩略图
  std::optional<std::string> thumbnail_path;
};

// 扫描配置
struct ScanOptions {
  std::vector<std::string> directories;
  bool recursive = true;
  bool generate_thumbnails = true;
  std::uint32_t thumbnail_max_width = 400;
  std::uint32_t thumbnail_max_height = 400;
  std::vector<std::string> supported_extensions = {".jpg",  ".jpeg", ".png", ".bmp",
                                                   ".webp", ".tiff", ".tif"};
};

// 扫描结果
struct ScanResult {
  int total_files;
  int new_items;
  int updated_items;
  int deleted_items;
  std::vector<std::string> errors;
  std::string scan_duration;  // 扫描耗时描述
};

// 媒体信息（从文件提取的基本信息）
struct MediaInfo {
  uint32_t width;
  uint32_t height;
  int64_t file_size;
  std::string mime_type;
  std::string detected_type;  // "photo", "video", "live_photo", "unknown"
};

// ============= RPC 请求/响应结构 =============

// 列表查询参数
struct ListMediaParams {
  std::optional<std::int32_t> page = 1;
  std::optional<std::int32_t> per_page = 50;
  std::optional<std::string> sort_by = "created_at";  // created_at, filename, file_size
  std::optional<std::string> sort_order = "desc";     // asc, desc
  std::optional<std::string> filter_type;             // photo, video, live_photo
  std::optional<std::string> search_query;            // 搜索文件名
};

// 列表查询响应
struct MediaListResponse {
  std::vector<MediaItem> items;
  std::int32_t total_count;
  std::int32_t current_page;
  std::int32_t per_page;
  std::int32_t total_pages;
};

// 获取单个媒体项参数
struct GetMediaParams {
  std::int64_t id;
};

// 扫描媒体参数
struct ScanMediaParams {
  std::vector<std::string> directories;
  std::optional<bool> recursive = true;
  std::optional<bool> generate_thumbnails = true;
  std::optional<std::uint32_t> thumbnail_max_width = 400;
  std::optional<std::uint32_t> thumbnail_max_height = 400;
};

// 获取缩略图参数
struct GetThumbnailParams {
  std::int64_t media_id;
  std::optional<std::uint32_t> width = 400;
  std::optional<std::uint32_t> height = 400;
};

// 删除媒体项参数
struct DeleteMediaParams {
  std::int64_t id;
  std::optional<bool> delete_file = false;  // 是否删除物理文件
};

// 通用操作结果
struct MediaOperationResult {
  bool success;
  std::string message;
  std::optional<std::int64_t> affected_count;
};

// 媒体统计信息
struct MediaStats {
  int total_count;
  int photo_count;
  int video_count;
  int live_photo_count;
  std::int64_t total_size;
  std::string oldest_item_date;
  std::string newest_item_date;
};

// 获取统计信息的参数（预留）
struct GetStatsParams {};

// 类型统计结果（用于数据库查询）
struct TypeCountResult {
  std::string type;
  int count;
};

}  // namespace Features::Media::Types
