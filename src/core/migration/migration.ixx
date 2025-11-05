module;

export module Core.Migration;

import std;
import Core.State;

namespace Core::Migration {

// 迁移脚本结构
export struct MigrationScript {
  std::string target_version;  // 目标版本号（4位格式），如 "1.1.0.0"
  std::string description;     // 迁移描述

  // 迁移执行函数：可以调用任何模块，执行任何操作
  std::function<std::expected<void, std::string>(Core::State::AppState&)> migration_fn;
};

// 获取上次保存的版本号
// 如果是首次启动（版本文件不存在），返回 "0.0.0.0"
// 如果读取失败，返回 unexpected 错误信息
export auto get_last_version() -> std::expected<std::string, std::string>;

// 保存当前版本号
export auto save_current_version(const std::string& version) -> std::expected<void, std::string>;

// 检查并执行迁移（如果需要）
// 返回 true 表示成功（无需迁移或迁移成功）
// 返回 false 表示迁移失败
export auto run_migration_if_needed(Core::State::AppState& app) -> bool;

}  // namespace Core::Migration
