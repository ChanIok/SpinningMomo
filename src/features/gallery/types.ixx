module;

export module Features.Gallery.Types;

import std;

namespace Features::Gallery::Types {

// ============= 核心数据类型 =============

export struct Asset {
  std::int64_t id;
  std::string name;
  // Gallery 内部路径不变量：
  // 一旦进入 DB / watcher / scan change 链路，path 就应当已经是
  // “absolute + lexical normal + generic slash”的内部路径语义。
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
  std::optional<std::string> hash;  // XXH3 内容指纹，大媒体使用固定区间采样
  std::optional<std::int64_t> root_id;
  std::optional<std::string> relative_path;
  std::optional<std::int64_t> folder_id;

  std::optional<std::int64_t> file_created_at;
  std::optional<std::int64_t> file_modified_at;

  std::int64_t created_at;
  std::int64_t updated_at;
};

export struct Folder {
  std::int64_t id;
  // 与 Asset.path 相同，Folder.path 也统一保存为 Gallery 内部规范路径。
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

export struct FolderTreeNode {
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
  bool is_network = false;
  std::int64_t asset_count = 0;
  std::vector<FolderTreeNode> children;
};

export struct IgnoreRule {
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

export struct Tag {
  std::int64_t id;
  std::string name;
  std::optional<std::int64_t> parent_id;
  int sort_order = 0;
  std::int64_t created_at;
  std::int64_t updated_at;
};

export struct BatchSelectionSummary {
  std::int64_t selected_count = 0;
  std::optional<int> rating;
  std::optional<bool> rejected_state;
  std::optional<std::string> description;
  std::vector<Tag> common_tags;
};

export struct TagTreeNode {
  std::int64_t id;
  std::string name;
  std::optional<std::int64_t> parent_id;
  int sort_order = 0;
  std::int64_t created_at;
  std::int64_t updated_at;
  std::int64_t asset_count = 0;  // 使用该标签（包含子标签）的资产总数
  std::vector<TagTreeNode> children;
};

export struct AssetTag {
  std::int64_t asset_id;
  std::int64_t tag_id;
  std::int64_t created_at;
};

export struct AssetMainColor {
  std::int64_t r = 0;
  std::int64_t g = 0;
  std::int64_t b = 0;
  double weight = 0.0;
};

// ============= 辅助数据类型 =============

export struct Info {
  std::uint32_t width;
  std::uint32_t height;
  std::int64_t size;
  std::string mime_type;
  std::string detected_type;
};

export struct Stats {
  int total_count = 0;
  int photo_count = 0;
  int video_count = 0;
  int live_photo_count = 0;
  std::int64_t total_size = 0;
  std::string oldest_item_date;
  std::string newest_item_date;
};

export struct HomeStats {
  int total_count = 0;
  int photo_count = 0;
  int video_count = 0;
  int live_photo_count = 0;
  std::int64_t total_size = 0;
  int today_added_count = 0;
};

export struct TagStats {
  std::int64_t tag_id;
  std::string tag_name;
  std::int64_t asset_count;  // 使用该标签的资产数量
};

export struct TypeCountResult {
  std::string type;
  int count;
};

export struct FolderHierarchy {
  std::string path;
  std::optional<std::string> parent_path;
  std::string name;
  int level = 0;
};

// ============= 扫描相关类型 =============

//  忽略规则（用于前端请求）
export struct ScanIgnoreRule {
  std::string pattern;
  std::string pattern_type = "glob";  // "glob" 或 "regex"
  std::string rule_type = "exclude";  // "exclude" 或 "include"
  std::optional<std::string> description;
};

export constexpr std::uint32_t kDefaultThumbnailShortEdge = 480;

export struct ScanOptions {
  std::string directory;
  std::optional<bool> force_reanalyze = false;
  std::optional<bool> rebuild_thumbnails = false;
  // 留空时统一回落到 Scanner::Common::default_supported_extensions()，避免多处维护默认列表。
  std::optional<std::vector<std::string>> supported_extensions;
  std::optional<std::vector<ScanIgnoreRule>> ignore_rules;
};

export struct ScanProgress {
  std::string stage;
  std::int64_t current = 0;
  std::int64_t total = 0;
  std::optional<double> percent;
  std::optional<std::string> message;
};

export enum class ScanChangeAction {
  UPSERT,
  REMOVE,
};

// 扫描输出的最小变化单元。
// 供运行时增量消费者（如 Infinity Nikki ScreenShot 硬链接同步）直接复用，
// 避免再次全量遍历文件系统推导“这次到底哪些文件变了”。
// REMOVE 表示监视根下该路径对应的文件已从磁盘消失；与索引中是否仍能删到一行资产无关。
export struct ScanChange {
  // Gallery 内部规范路径：absolute + lexical normal + generic slash。
  std::string path;
  ScanChangeAction action = ScanChangeAction::UPSERT;
};

export struct ScanResult {
  int total_files = 0;
  int new_items = 0;
  int updated_items = 0;
  int missing_items = 0;
  std::vector<std::string> errors = {};
  std::string scan_duration = "";
  // changes 供运行时增量消费者消费；
  // watcher 增量同步与 full scan 都可以填充，后者通常由扫描前后状态对账推导出来。
  std::vector<ScanChange> changes = {};
};

export enum class FileStatus { NEW, UNCHANGED, MODIFIED, NEEDS_HASH_CHECK };

export struct Metadata {
  std::int64_t id;
  std::string path;
  std::int64_t size;
  std::int64_t file_modified_at;
  std::string hash;
  std::optional<std::int64_t> missing_at;
};

export struct FileSystemInfo {
  // 扫描阶段产出的内部规范路径；后续 folder / cache / cleanup 链路都依赖它。
  std::filesystem::path path;
  std::int64_t size;
  std::int64_t file_modified_millis;
  std::int64_t file_created_millis;
  std::string hash;
};

export struct FileAnalysisResult {
  FileSystemInfo file_info;
  FileStatus status;
  std::optional<Metadata> existing_metadata;
};

// ============= RPC参数类型 =============

export struct ListResponse {
  std::vector<Asset> items;
  std::int32_t total_count;
  std::int32_t current_page;
  std::int32_t per_page;
  std::int32_t total_pages;
  std::optional<std::int64_t> active_asset_index;
};

export struct GetParams {
  std::int64_t id;
};

export struct GetAssetMainColorsParams {
  std::int64_t asset_id;
};

export struct AssetIdsParams {
  std::vector<std::int64_t> ids;
};

export struct DeleteAssetsParams {
  std::vector<std::int64_t> ids;
  std::string mode = "recycle_where_possible";
};

export struct MoveAssetsToFolderParams {
  std::vector<std::int64_t> ids;
  std::int64_t target_folder_id = 0;
};

export struct DeleteParams {
  std::int64_t id;
  std::optional<bool> delete_file = false;
};

export struct GetStatsParams {};

export struct ListAssetsParams {
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  // 分页和排序参数（复用ListParams的逻辑）
  std::optional<std::int32_t> page = 1;
  std::optional<std::int32_t> per_page = 500;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";
};

export struct OperationResult {
  bool success;
  std::string message;
  std::optional<std::int64_t> affected_count;
  std::optional<std::int64_t> failed_count = std::nullopt;
  std::optional<std::int64_t> not_found_count = std::nullopt;
  std::optional<std::int64_t> unchanged_count = std::nullopt;
};

export struct DeleteAssetsResult {
  bool success = false;
  std::string message;
  std::optional<std::int64_t> affected_count = 0;
  std::optional<std::int64_t> failed_count = 0;
  std::optional<std::int64_t> not_found_count = 0;
  std::optional<std::int64_t> unchanged_count = 0;
  std::int64_t recycle_bin_count = 0;
  std::int64_t permanent_count = 0;
};

export struct MissingAssetItem {
  std::int64_t id = 0;
  std::string name;
  std::string path;
  std::int64_t missing_at = 0;
};

export struct MissingAssetsResponse {
  std::vector<MissingAssetItem> items;
  std::int64_t total_count = 0;
  std::int64_t reclaimable_thumbnail_count = 0;
  std::int64_t reclaimable_thumbnail_bytes = 0;
};

export struct PurgeMissingAssetsParams {
  std::optional<std::vector<std::int64_t>> ids;
};

export struct PurgeMissingAssetsResult {
  bool success = false;
  std::string message;
  std::int64_t deleted_asset_count = 0;
  std::int64_t skipped_asset_count = 0;
  std::int64_t deleted_thumbnail_count = 0;
  std::int64_t released_thumbnail_bytes = 0;
  std::int64_t failed_thumbnail_count = 0;
};

// ============= 时间线相关类型 =============

export struct TimelineBucket {
  std::string month;  // "2024-10" 格式
  int count;          // 该月照片数量
};

export struct TimelineBucketsParams {
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  std::optional<std::string> sort_order = "desc";  // "asc" | "desc"
  std::optional<std::int64_t> active_asset_id;
  std::optional<std::int64_t> created_at_from;
  std::optional<std::int64_t> created_at_to;
  std::optional<std::string> type;
  std::optional<std::string> search;
  std::optional<std::vector<int>> ratings;  // 0 表示未评分，其它为 1~5 星
  std::optional<std::string> review_flag;
  std::optional<std::vector<std::int64_t>> tag_ids;
  std::optional<std::string> tag_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::string>> color_hexes;
  std::optional<std::string> color_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<double> color_distance = 18.0;
};

export struct TimelineBucketsResponse {
  std::vector<TimelineBucket> buckets;
  int total_count;  // 总照片数
  std::optional<std::int64_t> active_asset_index;
};

export struct GetAssetsByMonthParams {
  std::string month;  // "2024-10" 格式
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  std::optional<std::string> sort_order = "desc";  // "asc" | "desc"
  std::optional<std::int64_t> created_at_from;
  std::optional<std::int64_t> created_at_to;
  std::optional<std::string> type;
  std::optional<std::string> search;
  std::optional<std::vector<int>> ratings;  // 0 表示未评分，其它为 1~5 星
  std::optional<std::string> review_flag;
  std::optional<std::vector<std::int64_t>> tag_ids;
  std::optional<std::string> tag_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::string>> color_hexes;
  std::optional<std::string> color_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<double> color_distance = 18.0;
};

export struct GetAssetsByMonthResponse {
  std::string month;
  std::vector<Asset> assets;
  int count;
};

// ============= 统一查询相关类型 =============

export struct QueryAssetsFilters {
  std::optional<std::int64_t> folder_id;
  std::optional<bool> include_subfolders = false;
  std::optional<std::string> month;  // "2024-10" 格式
  std::optional<std::string> year;   // "2024" 格式
  std::optional<std::int64_t> created_at_from;
  std::optional<std::int64_t> created_at_to;
  std::optional<std::string> type;          // "photo" | "video" | "live_photo"
  std::optional<std::string> search;        // 搜索关键词
  std::optional<std::vector<int>> ratings;  // 0 表示未评分，其它为 1~5 星
  std::optional<std::string> review_flag;   // "none" | "picked" | "rejected"
  std::optional<std::vector<std::int64_t>> tag_ids;
  std::optional<std::string> tag_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<std::vector<std::string>> color_hexes;
  std::optional<std::string> color_match_mode = "any";  // "any" (OR) | "all" (AND)
  std::optional<double> color_distance = 18.0;
};

export struct QueryAssetsParams {
  QueryAssetsFilters filters;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";
  std::optional<std::int64_t> active_asset_id;
  // 分页是可选的：传page就分页，不传就返回所有结果
  std::optional<std::int32_t> page;
  std::optional<std::int32_t> per_page;
};

export struct AssetLayoutMetaItem {
  std::int64_t id;
  std::optional<std::int32_t> width;
  std::optional<std::int32_t> height;
};

export struct QueryAssetLayoutMetaParams {
  QueryAssetsFilters filters;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";
};

export struct QueryAssetLayoutMetaResponse {
  std::vector<AssetLayoutMetaItem> items;
  std::int32_t total_count;
};

// ============= 标签相关参数 =============

export struct CreateTagParams {
  std::string name;
  std::optional<std::int64_t> parent_id;
  std::optional<int> sort_order = 0;
};

export struct UpdateTagParams {
  std::int64_t id;
  std::optional<std::string> name;
  std::optional<std::int64_t> parent_id;
  std::optional<int> sort_order;
};

export struct AddTagsToAssetParams {
  std::int64_t asset_id;
  std::vector<std::int64_t> tag_ids;
};

export struct AddTagToAssetsParams {
  std::vector<std::int64_t> asset_ids;
  std::int64_t tag_id = 0;
};

export struct RemoveTagFromAssetsParams {
  std::vector<std::int64_t> asset_ids;
  std::int64_t tag_id = 0;
};

export struct RemoveTagsFromAssetParams {
  std::int64_t asset_id;
  std::vector<std::int64_t> tag_ids;
};

export struct GetAssetTagsParams {
  std::int64_t asset_id;
};

export struct BatchSelectionSummaryParams {
  std::vector<std::int64_t> asset_ids;
};

export struct UpdateAssetsReviewStateParams {
  std::vector<std::int64_t> asset_ids;
  std::optional<int> rating;
  std::optional<std::string> review_flag;
};

export struct UpdateAssetDescriptionParams {
  std::int64_t asset_id;
  std::optional<std::string> description;
};

export struct UpdateAssetsDescriptionParams {
  std::vector<std::int64_t> asset_ids;
  std::optional<std::string> description;
};

export struct GetTagStatsParams {};

}  // namespace Features::Gallery::Types
