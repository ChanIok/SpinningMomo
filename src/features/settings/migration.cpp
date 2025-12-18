module;

#include <rfl/json.hpp>

module Features.Settings.Migration;

import std;
import Utils.Logger;
import Features.Settings.Types;

namespace Features::Settings::Migration {

// 未来的迁移函数在此添加
// 示例：
// auto migrate_v1_to_v2(rfl::Generic::Object& settings)
//     -> std::expected<rfl::Generic::Object, std::string> {
//   settings["version"] = 2;
//   // 迁移逻辑
//   return settings;
// }

auto get_all_migration_functions() -> const std::unordered_map<int, MigrationFunction>& {
  static const std::unordered_map<int, MigrationFunction> functions = {
      // 未来可以添加：{1, migrate_v1_to_v2}, ...
  };
  return functions;
}

auto migrate_settings(const rfl::Generic::Object& settings, int source_version, int target_version)
    -> std::expected<rfl::Generic::Object, std::string> {
  // 如果没有指定目标版本，默认迁移到最新版本
  int actual_target_version =
      (target_version == -1) ? Types::CURRENT_SETTINGS_VERSION : target_version;

  // 如果已经达到或超过目标版本，直接返回
  if (source_version >= actual_target_version) {
    Logger().info("Settings already at version {}, no migration needed", source_version);
    return settings;
  }

  rfl::Generic::Object current_settings = settings;
  int current_version = source_version;

  Logger().info("Migrating settings from version {} to {}", source_version, actual_target_version);

  const auto& migration_functions = get_all_migration_functions();

  // 逐步升级到目标版本
  while (current_version < actual_target_version) {
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
      // 如果没有找到迁移函数，检查是否已经到达目标版本
      if (current_version >= actual_target_version) {
        break;
      }
      return std::unexpected("No migration function found for version " +
                             std::to_string(current_version));
    }
  }

  Logger().info("Settings migration completed: {} -> {}", source_version, actual_target_version);
  return current_settings;
}
}  // namespace Features::Settings::Migration