module;

export module Features.Gallery.Types;

import std;

export namespace Features::Gallery::Types {

// ============= 核心数据类型 =============

struct Asset {
  std::int64_t id;
  std::string name;
  std::string path;
  std::string type;  // photo, video, live_photo, unknown
  std::optional<std::string> dominant_color_hex;
  int rating = 0;
  std::string review_flag = "none";

  std::optional<std::string> description;
  std::optional<std::int32_t> width;
  std::optional<std::int32_t> height;
  std::optional<std::int64_t> size;
  std::optional<std::string> extension;
  std::string mime_type;
  std::optional<std::string> hash;  // xxh3哈希
  std::optional<std::int64_t> root_id;
  std::optional<std::string> relative_path;
  std::optional<std::int64_t> folder_id;

  std::optional<std::int64_t> file_created_at;
  std::optional<std::int64_t> file_modified_at;

  std::int64_t created_at;
  std::int64_t updated_at;
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

struct FolderTreeNode {
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
  std::int64_t asset_count = 0;
  std::vector<FolderTreeNode> children;
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

struct Tag {
  std::int64_t id;
  std::string name;
  std::optional<std::int64_t> parent_id;
  int sort_order = 0;
  std::int64_t created_at;
  std::int64_t updated_at;
};

struct TagTreeNode {
  std::int64_t id;
  std::string name;
  std::optional<std::int64_t> parent_id;
  int sort_order = 0;
  std::int64_t created_at;
  std::int64_t updated_at;
  std::int64_t asset_count = 0;  // 使用该标签（包含子标签）的资产总数
  std::vector<TagTreeNode> children;
};

struct AssetTag {
  std::int64_t asset_id;
  std::int64_t tag_id;
  std::int64_t created_at;
};

struct AssetMainColor {
  std::int64_t r = 0;
  std::int64_t g = 0;
  std::int64_t b = 0;
  double weight = 0.0;
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

struct HomeStats {
  int total_count = 0;
  int photo_count = 0;
  int video_count = 0;
  int live_photo_count = 0;
  std::int64_t total_size = 0;
  int today_added_count = 0;
};

struct TagStats {
  std::int64_t tag_id;
  std::string tag_name;
  std::int64_t asset_count;  // 使用该标签的资产数量
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
  std::optional<bool> generate_thumbnails = true;
  std::optional<std::uint32_t> thumbnail_short_edge = 480;
  // 留空时统一回落到 ScanCommon::default_supported_extensions()，避免多处维护默认列表。
  std::optional<std::vector<std::string>> supported_extensions;
  std::optional<std::vector<ScanIgnoreRule>> ignore_rules;
};

struct ScanProgress {
  std::string stage;
  std::int64_t current = 0;
  std::int64_t total = 0;
  std::optional<double> percent;
  std::optional<std::string> message;
};

enum class ScanChangeAction {
  UPSERT,
  REMOVE,
};

// 扫描输出的最小变化单元。
// 供运行时增量消费者（如 Infinity Nikki ScreenShot 硬链接同步）直接复用，
// 避免再次全量遍历文件系统推导“这次到底哪些文件变了”。
// REMOVE 表示监视根下该路径对应的文件已从磁盘消失；与索引中是否仍能删到一行资产无关。
struct ScanChange {
  std::string path;
  ScanChangeAction action = ScanChangeAction::UPSERT;
};

struct ScanResult {
  int total_files = 0;
  int new_items = 0;
  int updated_items = 0;
  int deleted_items = 0;
  std::vector<std::string> errors = {};
  std::string scan_duration = "";
  // changes 主要在 watcher 增量同步场景下填充；
  // 全量扫描允许为空，因为它关注的是“最终一致性”而非“逐文件变化集”。
  std::vector<ScanChange> changes = {};
};

enum class FileStatus { NEW, UNCHANGED, MODIFIED, NEEDS_HASH_CHECK, DELETED };

struct Metadata {
  std::int64_t id;
  std::string path;
  std::int64_t size;
  std::int64_t file_modified_at;
  std::string hash;
};

struct FileSystemInfo {
  std::filesystem::path path;
  std::int64_t size;
  std::int64_t file_modified_millis;
  std::int64_t file_created_millis;
  std::string hash;
};

struct FileAnalysisResult {
  FileSystemInfo file_info;
  FileStatus status;
  std::optional<Metadata> existing_metadata;
};

// ============= RPC参数类型 =============

struct ListResponse {
  std::vector<Asset> items;
  std::int32_t total_count;
  std::int32_t current_page;
  std::int32_t per_page;
  std::int32_t total_pages;
  std::optional<std::int64_t> active_asset_index;
};

struct PhotoMapPoint {
  std::int64_t asset_id;
  std::string name;
  std::optional<std::string> hash;
  std::optional<std::int64_t> file_created_at;
  double nikki_loc_x;
  double nikki_loc_y;
  std::optional<double> nikki_loc_z;
  // asset_index：该照片在“当前 gallery 排序结果集”里的 0-based 下标
  // 用于地图点击后原子地对齐灯箱 activeIndex，避免闪一下。
  std::int64_t asset_index;
};

struct GetParams {
  std::int64_t id;
};

struct GetInfinityNikkiDetailsParams {
  std::int64_t asset_id;
};

struct GetAssetMainColorsParams {
  std::int64_t asset_id;
};

struct AssetIdsParams {
  std::vector<std::int64_t> ids;
};

struct MoveAssetsToFolderParams {
  std::vector<std::int64_t> ids;
  std::int64_t target_folder_id = 0;
};

struct DeleteParams {
  std::int64_t id;
  std::optional<bool> delete_file = false;
};

struct GetStatsParams {};

struct ListAssetsParams {
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  // 分页和排序参数（复用ListParams的逻辑）
  std::optional<std::int32_t> page = 1;
  std::optional<std::int32_t> per_page = 50;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";
};

struct OperationResult {
  bool success;
  std::string message;
  std::optional<std::int64_t> affected_count;
  std::optional<std::int64_t> failed_count = std::nullopt;
  std::optional<std::int64_t> not_found_count = std::nullopt;
  std::optional<std::int64_t> unchanged_count = std::nullopt;
};

// ============= 时间线相关类型 =============

struct TimelineBucket {
  std::string month;  // "2024-10" 格式
  int count;          // 该月照片数量
};

struct TimelineBucketsParams {
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  std::optional<std::string> sort_order = "desc";  // "asc" | "desc"
  std::optional<std::int64_t> active_asset_id;
  std::optional<std::string> type;
  std::optional<std::string> search;
  std::optional<int> rating;
  std::optional<std::string> review_flag;
  std::optional<std::vector<std::int64_t>> tag_ids;
  std::optional<std::string> tag_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::int64_t>> cloth_ids;
  std::optional<std::string> cloth_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::string>> color_hexes;
  std::optional<std::string> color_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<double> color_distance = 18.0;
};

struct TimelineBucketsResponse {
  std::vector<TimelineBucket> buckets;
  int total_count;  // 总照片数
  std::optional<std::int64_t> active_asset_index;
};

struct GetAssetsByMonthParams {
  std::string month;  // "2024-10" 格式
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  std::optional<std::string> sort_order = "desc";  // "asc" | "desc"
  std::optional<std::string> type;
  std::optional<std::string> search;
  std::optional<int> rating;
  std::optional<std::string> review_flag;
  std::optional<std::vector<std::int64_t>> tag_ids;
  std::optional<std::string> tag_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::int64_t>> cloth_ids;
  std::optional<std::string> cloth_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::string>> color_hexes;
  std::optional<std::string> color_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<double> color_distance = 18.0;
};

struct GetAssetsByMonthResponse {
  std::string month;
  std::vector<Asset> assets;
  int count;
};

// ============= 统一查询相关类型 =============

struct QueryAssetsFilters {
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  std::optional<std::string> month;        // "2024-10" 格式
  std::optional<std::string> year;         // "2024" 格式
  std::optional<std::string> type;         // "photo" | "video" | "live_photo"
  std::optional<std::string> search;       // 搜索关键词
  std::optional<int> rating;               // 0 表示未评分，其它为 1~5 星
  std::optional<std::string> review_flag;  // "none" | "picked" | "rejected"
  std::optional<std::vector<std::int64_t>> tag_ids;
  std::optional<std::string> tag_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::int64_t>> cloth_ids;
  std::optional<std::string> cloth_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::string>> color_hexes;
  std::optional<std::string> color_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<double> color_distance = 18.0;
};

struct QueryAssetsParams {
  QueryAssetsFilters filters;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";
  std::optional<std::int64_t> active_asset_id;
  // 分页是可选的：传page就分页，不传就返回所有结果
  std::optional<std::int32_t> page;
  std::optional<std::int32_t> per_page;
};

struct AssetLayoutMetaItem {
  std::int64_t id;
  std::optional<std::int32_t> width;
  std::optional<std::int32_t> height;
};

struct QueryAssetLayoutMetaParams {
  QueryAssetsFilters filters;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";
};

struct QueryAssetLayoutMetaResponse {
  std::vector<AssetLayoutMetaItem> items;
  std::int32_t total_count;
};

struct QueryPhotoMapPointsParams {
  QueryAssetsFilters filters;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";  // "asc" | "desc"
};

struct InfinityNikkiExtractedParams {
  std::optional<std::string> camera_params;
  std::optional<std::int64_t> time_hour;
  std::optional<std::int64_t> time_min;
  std::optional<double> camera_focal_length;
  std::optional<double> rotation;
  std::optional<double> aperture_value;
  std::optional<std::int64_t> filter_id;
  std::optional<double> filter_strength;
  std::optional<double> vignette_intensity;
  std::optional<std::int64_t> light_id;
  std::optional<double> light_strength;
  std::optional<std::int64_t> vertical;
  std::optional<double> bloom_intensity;
  std::optional<double> bloom_threshold;
  std::optional<double> brightness;
  std::optional<double> exposure;
  std::optional<double> contrast;
  std::optional<double> saturation;
  std::optional<double> vibrance;
  std::optional<double> highlights;
  std::optional<double> shadow;
  std::optional<double> nikki_loc_x;
  std::optional<double> nikki_loc_y;
  std::optional<double> nikki_loc_z;
  std::optional<std::int64_t> nikki_hidden;
  std::optional<std::int64_t> pose_id;
};

struct InfinityNikkiUserRecord {
  std::string code_type;
  std::string code_value;
};

struct InfinityNikkiDetails {
  std::optional<InfinityNikkiExtractedParams> extracted;
  std::optional<InfinityNikkiUserRecord> user_record;
};

// ============= 标签相关参数 =============

struct CreateTagParams {
  std::string name;
  std::optional<std::int64_t> parent_id;
  std::optional<int> sort_order = 0;
};

struct UpdateTagParams {
  std::int64_t id;
  std::optional<std::string> name;
  std::optional<std::int64_t> parent_id;
  std::optional<int> sort_order;
};

struct AddTagsToAssetParams {
  std::int64_t asset_id;
  std::vector<std::int64_t> tag_ids;
};

struct RemoveTagsFromAssetParams {
  std::int64_t asset_id;
  std::vector<std::int64_t> tag_ids;
};

struct GetAssetTagsParams {
  std::int64_t asset_id;
};

struct UpdateAssetsReviewStateParams {
  std::vector<std::int64_t> asset_ids;
  std::optional<int> rating;
  std::optional<std::string> review_flag;
};

struct UpdateAssetDescriptionParams {
  std::int64_t asset_id;
  std::optional<std::string> description;
};

struct SetInfinityNikkiUserRecordParams {
  std::int64_t asset_id;
  std::string code_type;
  std::optional<std::string> code_value;
};

struct GetTagStatsParams {};

}  // namespace Features::Gallery::Types
