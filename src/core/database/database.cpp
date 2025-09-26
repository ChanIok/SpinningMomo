module;

module Core.Database;

import std;
import Core.Database.DataMapper;
import Core.Database.State;
import Core.Database.Types;
import Utils.Logger;
import Utils.Path;
import <SQLiteCpp/SQLiteCpp.h>;

namespace Core::Database {

// 获取当前线程的数据库连接。如果连接不存在，则创建它。
auto get_connection(const std::filesystem::path& db_path) -> SQLite::Database& {
  if (!thread_connection) {
    thread_connection = std::make_unique<SQLite::Database>(
        db_path.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    // 设置 WAL 模式以提高并发性能
    thread_connection->exec("PRAGMA journal_mode=WAL;");
    thread_connection->exec("PRAGMA synchronous=NORMAL;");
  }
  return *thread_connection;
}

// 初始化数据库，现在只负责配置路径和进行初始连接测试
auto initialize(State::DatabaseState& state, const std::filesystem::path& db_path)
    -> std::expected<void, std::string> {
  try {
    // 保存数据库路径
    state.db_path = db_path;

    // 确保父目录存在
    if (db_path.has_parent_path()) {
      std::filesystem::create_directories(db_path.parent_path());
    }

    // 在主线程上尝试创建一个连接，以验证数据库路径和配置
    get_connection(db_path);

    Logger().info("Database configured successfully: {}", db_path.string());
    return {};

  } catch (const SQLite::Exception& e) {
    Logger().error("Cannot open database: {} - Error: {}", db_path.string(), e.what());
    return std::unexpected(std::string("Cannot open database: ") + e.what());
  } catch (const std::exception& e) {
    Logger().error("Cannot open database: {} - Error: {}", db_path.string(), e.what());
    return std::unexpected(std::string("Cannot open database: ") + e.what());
  }
}

// 关闭当前线程的数据库连接
auto close(State::DatabaseState& state) -> void {
  if (thread_connection) {
    thread_connection.reset();
    Logger().info("Database connection closed for the current thread: {}", state.db_path.string());
  }
}

// 执行非查询操作 (INSERT, UPDATE, DELETE)
auto execute(State::DatabaseState& state, const std::string& sql)
    -> std::expected<void, std::string> {
  try {
    auto& connection = get_connection(state.db_path);
    connection.exec(sql);
    return {};
  } catch (const SQLite::Exception& e) {
    Logger().error("Failed to execute statement: {} - Error: {}", sql, e.what());
    return std::unexpected(std::string("Failed to execute statement: ") + e.what());
  }
}

auto execute(State::DatabaseState& state, const std::string& sql,
             const std::vector<Types::DbParam>& params) -> std::expected<void, std::string> {
  try {
    auto& connection = get_connection(state.db_path);
    SQLite::Statement query(connection, sql);

    // 绑定参数
    for (size_t i = 0; i < params.size(); ++i) {
      const auto& param = params[i];
      int param_index = static_cast<int>(i + 1);  // SQLite 参数是 1-based 索引

      std::visit(
          [&query, param_index](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
              query.bind(param_index);  // 绑定 NULL
            } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
              query.bind(param_index, arg.data(), static_cast<int>(arg.size()));  // 绑定 BLOB
            } else {
              // 通过重载方法绑定 int64_t, double, std::string
              query.bind(param_index, arg);
            }
          },
          param);
    }

    query.exec();  // 对不返回结果的语句使用 exec()
    return {};
  } catch (const SQLite::Exception& e) {
    Logger().error("Failed to execute statement: {} - Error: {}", sql, e.what());
    return std::unexpected(std::string("Failed to execute statement: ") + e.what());
  }
}

}  // namespace Core::Database