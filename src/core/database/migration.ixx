module;

export module Core.Database.Migration;

import std;
import Core.Database.State;

namespace Core::Database::Migration {
// 简单的迁移结构
export struct MigrationScript {
  int version;
  std::string description;
  std::vector<std::string> sqls;
};

// 执行迁移的主函数
export auto run_migrations(State::DatabaseState& db_state) -> bool;

}  // namespace Core::Database::Migration