module;

export module Features.Gallery.Types;

import std;

export namespace Features::Gallery::Types {

// ============= 核心数据类型 =============

struct Asset {
  std::int64_t id;
  std::string name;
  std::string filepath;
  std::string type;  // photo, video, live_photo, unknown

  std::optional<std::int32_t> width;
  std::optional<std::int32_t> height;
  std::optional<std::int64_t> size;
  std::string mime_type;
  std::optional<std::string> hash;  // xxh3哈希
  std::optional<std::int64_t> folder_id;

  std::optional<std::int64_t> file_created_at;
  std::optional<std::int64_t> file_modified_at;

  std::int64_t created_at;
  std::int64_t updated_at;
  std::optional<std::int64_t> deleted_at;
};

struct Folder {
  std::int64_t id;
  std::string path;
  std::optional<std::int64_t> parent_id;
  std::string name;
  std::optional<std::string> display_name;
  std::optional<std::int64_t> cover_asset_id;
  int sort_order = 0;
  int is_hidden = 0;
  std::int64_t created_at;
  std::int64_t updated_at;
};

struct IgnoreRule {
  std::int64_t id;
  std::optional<std::int64_t> folder_id;
  std::string rule_pattern;
  std::string pattern_type = "glob";
  std::string rule_type = "exclude";
  int is_enabled = 1;
  std::optional<std::string> description;
  std::int64_t created_at;
  std::int64_t updated_at;
};

// ============= 辅助数据类型 =============

struct Info {
  std::uint32_t width;
  std::uint32_t height;
  std::int64_t size;
  std::string mime_type;
  std::string detected_type;
};

struct Stats {
  int total_count = 0;
  int photo_count = 0;
  int video_count = 0;
  int live_photo_count = 0;
  std::int64_t total_size = 0;
  std::string oldest_item_date;
  std::string newest_item_date;
};

struct TypeCountResult {
  std::string type;
  int count;
};

struct FolderHierarchy {
  std::string path;
  std::optional<std::string> parent_path;
  std::string name;
  int level = 0;
};

// ============= 扫描相关类型 =============

//  忽略规则（用于前端请求）
struct ScanIgnoreRule {
  std::string pattern;
  std::string pattern_type = "glob";  // "glob" 或 "regex"
  std::string rule_type = "exclude";  // "exclude" 或 "include"
  std::optional<std::string> description;
};

struct ScanOptions {
  std::string directory;
  bool generate_thumbnails = true;
  std::uint32_t thumbnail_max_width = 400;
  std::uint32_t thumbnail_max_height = 400;
  std::vector<std::string> supported_extensions = {".jpg",  ".jpeg", ".png", ".bmp",
                                                   ".webp", ".tiff", ".tif"};
  std::vector<ScanIgnoreRule> ignore_rules;
};

struct ScanResult {
  int total_files = 0;
  int new_items = 0;
  int updated_items = 0;
  int deleted_items = 0;
  std::vector<std::string> errors = {};
  std::string scan_duration = "";
};

enum class FileStatus { NEW, UNCHANGED, MODIFIED, NEEDS_HASH_CHECK, DELETED };

struct Metadata {
  int64_t id;
  std::string filepath;
  int64_t size;
  std::int64_t file_modified_at;
  std::string hash;
};

struct FileSystemInfo {
  std::filesystem::path filepath;
  int64_t size;
  std::int64_t file_modified_millis;
  std::int64_t file_created_millis;
  std::string hash;
};

struct FileAnalysisResult {
  FileSystemInfo file_info;
  FileStatus status;
  std::optional<Metadata> existing_metadata;
};

struct ProcessingBatchResult {
  std::vector<Asset> new_assets;
  std::vector<Asset> updated_assets;
  std::vector<std::string> generated_thumbnails;
  std::vector<std::string> errors;
};

// ============= RPC参数类型 =============

struct ListParams {
  std::optional<std::int32_t> page = 1;
  std::optional<std::int32_t> per_page = 50;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";
  std::optional<std::string> filter_type;
  std::optional<std::string> search_query;
};

struct ListResponse {
  std::vector<Asset> items;
  std::int32_t total_count;
  std::int32_t current_page;
  std::int32_t per_page;
  std::int32_t total_pages;
};

struct GetParams {
  std::int64_t id;
};

struct ScanParams {
  std::string directory;
  bool generate_thumbnails = true;
  std::uint32_t thumbnail_max_width = 400;
  std::uint32_t thumbnail_max_height = 400;
  std::vector<ScanIgnoreRule> ignore_rules;
};

struct GetThumbnailParams {
  std::int64_t asset_id;
  std::optional<std::uint32_t> width = 400;
  std::optional<std::uint32_t> height = 400;
};

struct DeleteParams {
  std::int64_t id;
  std::optional<bool> delete_file = false;
};

struct GetStatsParams {};

struct OperationResult {
  bool success;
  std::string message;
  std::optional<std::int64_t> affected_count;
};

}  // namespace Features::Gallery::Types
