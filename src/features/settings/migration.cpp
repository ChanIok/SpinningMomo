module;

#include <rfl/json.hpp>

module Features.Settings.Migration;

import std;
import Utils.Logger;
import Features.Settings.Types;

namespace Features::Settings::Migration {

// v1到v2的迁移函数
auto migrate_v1_to_v2(rfl::Generic::Object& settings)
    -> std::expected<rfl::Generic::Object, std::string> {
  try {
    // 1. 更新版本号
    settings["version"] = 2;

    // 2. 添加新字段示例（根据实际需要修改）
    // settings["new_field"] = "default_value";

    Logger().info("Successfully migrated settings from v1 to v2");
    return settings;
  } catch (const std::exception& e) {
    Logger().error("Failed to migrate settings from v1 to v2: {}", e.what());
    return std::unexpected("Migration failed: " + std::string(e.what()));
  }
}

// 创建版本到迁移函数的映射
static const std::unordered_map<
    int, std::function<std::expected<rfl::Generic::Object, std::string>(rfl::Generic::Object&)>>
    migration_functions = {
        {1, migrate_v1_to_v2},
        // 未来可以继续添加：{2, migrate_v2_to_v3}, ...
};

// 执行设置迁移
auto migrate_settings(const rfl::Generic::Object& settings, int source_version)
    -> std::expected<rfl::Generic::Object, std::string> {
  // 如果已经是最新版本，直接返回
  if (source_version >= Types::CURRENT_SETTINGS_VERSION) {
    return settings;
  }

  rfl::Generic::Object current_settings = settings;
  int current_version = source_version;

  // 逐步升级到最新版本
  while (current_version < Types::CURRENT_SETTINGS_VERSION) {
    // 根据当前版本选择相应的迁移函数
    auto it = migration_functions.find(current_version);
    if (it != migration_functions.end()) {
      auto result = it->second(current_settings);
      if (!result) {
        return std::unexpected("Migration failed from v" + std::to_string(current_version) +
                               " to v" + std::to_string(current_version + 1) + ": " +
                               result.error());
      }
      current_settings = result.value();
      current_version++;
    } else {
      return std::unexpected("Unsupported version: " + std::to_string(current_version));
    }
  }

  Logger().info("Settings migrated from version {} to {}", source_version,
                Types::CURRENT_SETTINGS_VERSION);
  return current_settings;
}
}  // namespace Features::Settings::Migration