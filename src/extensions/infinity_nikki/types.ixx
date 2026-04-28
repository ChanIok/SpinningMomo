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

struct InfinityNikkiMapArea {
  std::string auto_world_id;
  std::optional<std::string> user_world_id;
  std::string world_id;
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
