module;

#include <SQLiteCpp/SQLiteCpp.h>

module Core.Database;

import std;
import Core.Database.DataMapper;
import Core.Database.State;
import Core.Database.Types;
import Utils.Logger;
import Utils.Path;

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

// 查询返回多个结果 (SELECT)
template <typename T>
auto query(State::DatabaseState& state, const std::string& sql,
           const std::vector<Types::DbParam>& params)
    -> std::expected<std::vector<T>, std::string> {
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

    std::vector<T> results;
    while (query.executeStep()) {
      auto mapped_object = DataMapper::from_statement<T>(query);
      if (mapped_object) {
        results.push_back(std::move(*mapped_object));
      } else {
        return std::unexpected("Failed to map row to object: " + mapped_object.error());
      }
    }
    return results;
  } catch (const SQLite::Exception& e) {
    return std::unexpected("SQLite error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Generic error: " + std::string(e.what()));
  }
}

// 查询返回单个结果 (SELECT)
template <typename T>
auto query_single(State::DatabaseState& state, const std::string& sql,
                  const std::vector<Types::DbParam>& params)
    -> std::expected<std::optional<T>, std::string> {
  auto results = query<T>(state, sql, params);
  if (!results) {
    return std::unexpected(results.error());
  }
  if (results->empty()) {
    return std::optional<T>{};
  }
  if (results->size() > 1) {
    // 或记录警告，具体取决于所需的严格性
    return std::unexpected("Query for single result returned multiple rows.");
  }
  return std::move(results->front());
}

// 查询单个标量值
template <typename T>
auto query_scalar(State::DatabaseState& state, const std::string& sql,
                  const std::vector<Types::DbParam>& params)
    -> std::expected<std::optional<T>, std::string> {
  try {
    auto& connection = get_connection(state.db_path);
    SQLite::Statement query(connection, sql);

    for (size_t i = 0; i < params.size(); ++i) {
      const auto& param = params[i];
      int param_index = static_cast<int>(i + 1);
      std::visit(
          [&query, param_index](auto&& arg) {
            using U = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<U, std::monostate>) {
              query.bind(param_index);
            } else if constexpr (std::is_same_v<U, std::vector<std::uint8_t>>) {
              query.bind(param_index, arg.data(), static_cast<int>(arg.size()));
            } else {
              query.bind(param_index, arg);
            }
          },
          param);
    }

    if (query.executeStep()) {
      SQLite::Column col = query.getColumn(0);
      if (col.isNull()) {
        return std::optional<T>{};
      }
      if constexpr (std::is_same_v<T, int>) {
        return col.getInt();
      } else if constexpr (std::is_same_v<T, int64_t>) {
        return col.getInt64();
      } else if constexpr (std::is_same_v<T, double>) {
        return col.getDouble();
      } else if constexpr (std::is_same_v<T, std::string>) {
        return col.getString();
      } else {
        // 不支持的类型
        static_assert(sizeof(T) == 0, "Unsupported type for query_scalar");
      }
    }

    return std::optional<T>{};  // 没有结果
  } catch (const SQLite::Exception& e) {
    return std::unexpected("SQLite error: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Generic error: " + std::string(e.what()));
  }
}

// 为 query_scalar 显式实例化常用类型
template auto query_scalar<int>(State::DatabaseState&, const std::string&,
                                const std::vector<Types::DbParam>&)
    -> std::expected<std::optional<int>, std::string>;
template auto query_scalar<int64_t>(State::DatabaseState&, const std::string&,
                                    const std::vector<Types::DbParam>&)
    -> std::expected<std::optional<int64_t>, std::string>;
template auto query_scalar<std::string>(State::DatabaseState&, const std::string&,
                                        const std::vector<Types::DbParam>&)
    -> std::expected<std::optional<std::string>, std::string>;

}  // namespace Core::Database