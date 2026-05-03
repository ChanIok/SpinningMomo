module;

export module Extensions.InfinityNikki.Types;

import std;
import Features.Gallery.Types;

export namespace Extensions::InfinityNikki {

struct InfinityNikkiGameDirResult {
  std::optional<std::string> game_dir;  // 游戏目录，null 表示未找到
  bool config_found{false};             // 配置文件是否存在
  bool game_dir_found{false};           // gameDir 字段是否存在
  std::string message;                  // 状态描述信息
};

struct InfinityNikkiExtractPhotoParamsRequest {
  std::optional<bool> only_missing = true;
  std::optional<std::int64_t> folder_id;
  std::optional<std::string> uid_override;
};

struct InfinityNikkiSilentExtractPhotoParamsRequest {
  std::vector<std::int64_t> candidate_asset_ids;
};

struct InfinityNikkiExtractPhotoParamsForFolderRequest {
  std::int64_t folder_id = 0;
  std::string uid;
  std::optional<bool> only_missing = false;
};

struct InfinityNikkiExtractPhotoParamsProgress {
  std::string stage;
  std::int64_t current = 0;
  std::int64_t total = 0;
  std::optional<double> percent;
  std::optional<std::string> message;
};

struct InfinityNikkiExtractPhotoParamsResult {
  std::int32_t candidate_count = 0;
  std::int32_t processed_count = 0;
  std::int32_t saved_count = 0;
  std::int32_t skipped_count = 0;
  std::int32_t failed_count = 0;
  std::int32_t clothes_rows_written = 0;
  std::vector<std::string> errors = {};
};

struct InfinityNikkiInitializeMediaHardlinksProgress {
  std::string stage;
  std::int64_t current = 0;
  std::int64_t total = 0;
  std::optional<double> percent;
  std::optional<std::string> message;
};

struct InfinityNikkiInitializeMediaHardlinksResult {
  std::int32_t source_count = 0;
  std::int32_t created_count = 0;
  std::int32_t updated_count = 0;
  std::int32_t removed_count = 0;
  std::int32_t ignored_count = 0;
  std::vector<std::string> errors = {};
};

struct PhotoMapPoint {
  std::int64_t asset_id;
  std::string name;
  std::optional<std::string> hash;
  std::optional<std::int64_t> file_created_at;
  double nikki_loc_x;
  double nikki_loc_y;
  std::optional<double> nikki_loc_z;
  // lat/lng：由后端根据远端配置将游戏坐标转换后的地图经纬度，前端直接使用。
  double lat;
  double lng;
  std::string world_id;
  std::string official_world_id;
  // asset_index：该照片在“当前 gallery 排序结果集”里的 0-based 下标
  // 用于地图点击后原子地对齐灯箱 activeIndex，避免闪一下。
  std::int64_t asset_index;
};

struct QueryPhotoMapPointsParams {
  Features::Gallery::Types::QueryAssetsFilters filters;
  std::optional<std::string> sort_by = "created_at";
  std::optional<std::string> sort_order = "desc";  // "asc" | "desc"
  std::string world_id;
};

struct GetInfinityNikkiDetailsParams {
  std::int64_t asset_id;
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
  std::optional<std::string> dye_code;
  std::optional<std::string> home_building_code;
  std::optional<std::string> world_id;
};

// 远端地图配置相关类型 —— 从 api.infinitymomo.com/api/v1/map.json 拉取，
// 用于替代原先硬编码在 C++ 和前端的世界区域 polygon、坐标转换参数和世界名称。

// 世界名称的多语言支持（可选字段，缺失时前端回退到 world_id）。
struct InfinityNikkiMapLocalizedName {
  std::optional<std::string> zh;
  std::optional<std::string> en;
};

// 坐标变换参数：map_x = game_x * x_scale + x_bias, map_y 同理。
// JSON 中使用 camelCase（如 xScale），C++ 结构体使用 snake_case，
// 由 rfl::SnakeCaseToCamelCase 在反序列化时自动映射。
struct InfinityNikkiMapCoordinateProfile {
  double x_scale = 1.0;
  double x_bias = 0.0;
  double y_scale = 1.0;
  double y_bias = 0.0;
};

struct InfinityNikkiMapPolygonPoint {
  double x = 0.0;
  double y = 0.0;
};

struct InfinityNikkiMapZRange {
  std::optional<double> min;
  std::optional<double> max;
};

// 世界区域判定规则：多边形 + 可选的 Z 轴范围。
// 若游戏坐标落入 polygon 且 z 在 [min, max] 内，则认为属于该世界。
struct InfinityNikkiMapWorldRule {
  std::vector<InfinityNikkiMapPolygonPoint> polygon;
  std::optional<InfinityNikkiMapZRange> z_range;
};

// 单个地图世界：包含内部 world_id、官方 world_id（带版本号，如 "1.1"）、
// 坐标变换参数和多条区域判定规则（按顺序匹配，首条命中即归属该世界）。
struct InfinityNikkiMapWorld {
  std::string world_id;
  std::string official_world_id;
  InfinityNikkiMapLocalizedName name;
  InfinityNikkiMapCoordinateProfile coordinate;
  std::vector<InfinityNikkiMapWorldRule> rules;
};

// 远端地图配置的顶层结构。
// schema_version 目前固定为 1；default_world_id 用于坐标不属于任何 polygon 时的回退。
struct InfinityNikkiMapConfig {
  std::int64_t schema_version = 1;
  std::string default_world_id;
  std::vector<InfinityNikkiMapWorld> worlds;
};

// 照片所属地图区域的解析结果。
// auto_* 是根据坐标自动推断的世界，user_world_id 是用户手动选择的覆盖值，
// world_id / official_world_id 是最终生效值（用户选择优先）。
struct InfinityNikkiMapArea {
  std::string auto_world_id;                 // 自动推断的内部 world_id
  std::string auto_official_world_id;        // 自动推断的官方 world_id（带版本号）
  std::optional<std::string> user_world_id;  // 用户手动设置的 world_id（可选）
  std::string world_id;                      // 最终生效的内部 world_id
  std::string official_world_id;             // 最终生效的官方 world_id
};

struct InfinityNikkiDetails {
  std::optional<InfinityNikkiExtractedParams> extracted;
  std::optional<InfinityNikkiUserRecord> user_record;
  std::optional<InfinityNikkiMapArea> map_area;
};

struct GetInfinityNikkiMetadataNamesParams {
  std::optional<std::int64_t> filter_id;
  std::optional<std::int64_t> pose_id;
  std::optional<std::int64_t> light_id;
  std::optional<std::string> locale = "zh-CN";
};

struct InfinityNikkiMetadataNames {
  std::optional<std::string> filter_name;
  std::optional<std::string> pose_name;
  std::optional<std::string> light_name;
};

struct SetInfinityNikkiUserRecordParams {
  std::int64_t asset_id;
  std::string code_type;
  std::optional<std::string> code_value;
};

struct PreviewInfinityNikkiSameOutfitDyeCodeFillParams {
  std::int64_t asset_id;
};

struct InfinityNikkiSameOutfitDyeCodeFillPreview {
  bool source_has_outfit_dye_state = false;
  std::int64_t matched_count = 0;
  std::int64_t fillable_count = 0;
  std::int64_t recorded_count = 0;
};

struct FillInfinityNikkiSameOutfitDyeCodeParams {
  std::int64_t asset_id;
  std::string code_value;
};

struct InfinityNikkiSameOutfitDyeCodeFillResult {
  bool success = false;
  std::string message;
  bool source_has_outfit_dye_state = false;
  std::int64_t matched_count = 0;
  std::int64_t affected_count = 0;
  std::int64_t skipped_existing_count = 0;
  std::int64_t updated_existing_count = 0;
};

struct SetInfinityNikkiWorldRecordParams {
  std::int64_t asset_id;
  std::optional<std::string> world_id;
};

}  // namespace Extensions::InfinityNikki
