module;

export module Extensions.InfinityNikki.Types;

import std;

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

struct InfinityNikkiInitializeScreenshotHardlinksProgress {
  std::string stage;
  std::int64_t current = 0;
  std::int64_t total = 0;
  std::optional<double> percent;
  std::optional<std::string> message;
};

struct InfinityNikkiInitializeScreenshotHardlinksResult {
  std::int32_t source_count = 0;
  std::int32_t created_count = 0;
  std::int32_t updated_count = 0;
  std::int32_t removed_count = 0;
  std::int32_t ignored_count = 0;
  std::vector<std::string> errors = {};
};

}  // namespace Extensions::InfinityNikki
