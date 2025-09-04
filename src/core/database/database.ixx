module;

#include <SQLiteCpp/SQLiteCpp.h>

export module Core.Database;

import std;
import Core.Database.State;
import Core.Database.Types;

namespace Core::Database {

// 线程局部存储，为每个线程维护一个独立的数据库连接
thread_local std::unique_ptr<SQLite::Database> thread_connection;

// 获取当前线程的数据库连接。如果连接不存在，则创建它。
auto get_connection(const std::filesystem::path& db_path) -> SQLite::Database&;

// 初始化数据库连接
export auto initialize(State::DatabaseState& state, const std::filesystem::path& db_path)
    -> std::expected<void, std::string>;

// 关闭数据库连接，理论上非必要，thread_connection 会在 thread_exit 时自动关闭
export auto close(State::DatabaseState& state) -> void;

// 执行非查询操作 (INSERT, UPDATE, DELETE)
export auto execute(State::DatabaseState& state, const std::string& sql)
    -> std::expected<void, std::string>;
export auto execute(State::DatabaseState& state, const std::string& sql,
                    const std::vector<Types::DbParam>& params) -> std::expected<void, std::string>;

// 查询返回多个结果 (SELECT)
export template <typename T>
auto query(State::DatabaseState& state, const std::string& sql,
           const std::vector<Types::DbParam>& params = {}) -> std::expected<std::vector<T>, std::string>;

// 查询返回单个结果 (SELECT)
export template <typename T>
auto query_single(State::DatabaseState& state, const std::string& sql,
                  const std::vector<Types::DbParam>& params = {})
    -> std::expected<std::optional<T>, std::string>;

// 查询返回单个标量值
export template <typename T>
auto query_scalar(State::DatabaseState& state, const std::string& sql,
                  const std::vector<Types::DbParam>& params = {})
    -> std::expected<std::optional<T>, std::string>;

// 事务管理
export template <typename Func>
auto execute_transaction(State::DatabaseState& state, Func&& func)
    -> std::expected<void, std::string> {
  try {
    auto& connection = get_connection(state.db_path);
    SQLite::Transaction transaction(connection);

    // 执行用户提供的事务逻辑
    if (auto result = func(state); !result) {
      // 如果函数返回错误，事务会在transaction析构时自动回滚
      return result;
    }

    // 提交事务
    transaction.commit();
    return {};
  } catch (const SQLite::Exception& e) {
    // 异常发生时，transaction析构会自动回滚
    return std::unexpected("Transaction failed: " + std::string(e.what()));
  } catch (const std::exception& e) {
    return std::unexpected("Transaction error: " + std::string(e.what()));
  }
}

}  // namespace Core::Database