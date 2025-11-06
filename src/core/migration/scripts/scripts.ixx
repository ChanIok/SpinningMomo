module;

export module Core.Migration.Scripts;

import std;
import Core.State;

namespace Core::Migration::Scripts {

// MigrationScript 定义
export struct MigrationScript {
  std::string target_version;
  std::string description;
  std::function<std::expected<void, std::string>(Core::State::AppState&)> migration_fn;
};

// 获取所有迁移脚本
export auto get_all_migrations() -> const std::vector<MigrationScript>&;

}  // namespace Core::Migration::Scripts
