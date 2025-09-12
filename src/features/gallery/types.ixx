module;

export module Features.Gallery.Types;

import std;

export namespace Features::Gallery::Types {

// 资产项核心结构
struct Asset {
  std::int64_t id;
  std::string name;
  std::string filepath;       // 完整路径
  std::string relative_path;  // 相对于监控目录的路径
  std::string type;           // "photo", "video", "live_photo", "unknown"

  // 基本信息（来自 Utils::Image::get_image_info）
  std::optional<std::int32_t> width;
  std::optional<std::int32_t> height;
  std::optional<std::int64_t> size;
  std::string mime_type;
  std::optional<std::string> hash;  // xxh3哈希值，用于快速比对

  // 时间信息
  std::string created_at;                 // 文件创建时间
  std::string updated_at;                 // 数据库更新时间
  std::optional<std::string> deleted_at;  // 软删除
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

// 资产信息（从文件提取的基本信息）
struct Info {
  uint32_t width;
  uint32_t height;
  int64_t size;
  std::string mime_type;
  std::string detected_type;  // "photo", "video", "live_photo", "unknown"
};

// ============= RPC 请求/响应结构 =============

// 列表查询参数
struct ListParams {
  std::optional<std::int32_t> page = 1;
  std::optional<std::int32_t> per_page = 50;
  std::optional<std::string> sort_by = "created_at";  // created_at, name, size
  std::optional<std::string> sort_order = "desc";     // asc, desc
  std::optional<std::string> filter_type;             // photo, video, live_photo
  std::optional<std::string> search_query;            // 搜索文件名
};

// 列表查询响应
struct ListResponse {
  std::vector<Asset> items;
  std::int32_t total_count;
  std::int32_t current_page;
  std::int32_t per_page;
  std::int32_t total_pages;
};

// 获取单个资产项参数
struct GetParams {
  std::int64_t id;
};

// 扫描资产参数
struct ScanParams {
  std::vector<std::string> directories;
  std::optional<bool> recursive = true;
  std::optional<bool> generate_thumbnails = true;
  std::optional<std::uint32_t> thumbnail_max_width = 400;
  std::optional<std::uint32_t> thumbnail_max_height = 400;
};

// 获取缩略图参数
struct GetThumbnailParams {
  std::int64_t asset_id;
  std::optional<std::uint32_t> width = 400;
  std::optional<std::uint32_t> height = 400;
};

// 删除资产项参数
struct DeleteParams {
  std::int64_t id;
  std::optional<bool> delete_file = false;  // 是否删除物理文件
};

// 通用操作结果
struct OperationResult {
  bool success;
  std::string message;
  std::optional<std::int64_t> affected_count;
};

// 资产统计信息
struct Stats {
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

// ============= 多线程扫描专用类型 =============

// 内存中的资产元数据（用于快速比对）
struct Metadata {
  int64_t id;
  std::string filepath;
  int64_t size;
  std::string last_modified;
  std::optional<std::string> hash;  // xxh3哈希
};

// 文件系统信息
struct FileSystemInfo {
  std::filesystem::path filepath;
  int64_t size;
  std::filesystem::file_time_type last_write_time;
  std::string last_modified_str;
  std::optional<std::string> hash;  // xxh3哈希（在发现阶段为空，后续按需计算）
};

// 文件状态枚举
enum class FileStatus {
  NEW,               // 新文件
  UNCHANGED,         // 无变化
  MODIFIED,          // 已修改
  NEEDS_HASH_CHECK,  // 需要进行哈希校验（大小/时间变化，需进一步确认）
  DELETED            // 数据库中存在但文件系统中不存在
};

// 文件分析结果
struct FileAnalysisResult {
  FileSystemInfo file_info;
  FileStatus status;
  std::optional<Metadata> existing_metadata;
};

// 处理批次结果
struct ProcessingBatchResult {
  std::vector<Asset> new_assets;
  std::vector<Asset> updated_assets;
  std::vector<std::string> generated_thumbnails;
  std::vector<std::string> errors;
};

// 内存缓存类型
using Cache = std::unordered_map<std::string, Metadata>;  // key: filepath

}  // namespace Features::Gallery::Types
