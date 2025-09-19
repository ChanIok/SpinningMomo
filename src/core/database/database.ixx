module;

#include <SQLiteCpp/SQLiteCpp.h>

export module Core.Database;

import std;
import Core.Database.State;
import Core.Database.Types;
import Core.Database.DataMapper;

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
           const std::vector<Types::DbParam>& params = {})
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
export template <typename T>
auto query_single(State::DatabaseState& state, const std::string& sql,
                  const std::vector<Types::DbParam>& params = {})
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

// 查询返回单个标量值
export template <typename T>
auto query_scalar(State::DatabaseState& state, const std::string& sql,
                  const std::vector<Types::DbParam>& params = {})
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

// 批量INSERT操作（自动分批处理）
export template <typename T, typename ParamExtractor>
auto execute_batch_insert(State::DatabaseState& state, const std::string& insert_prefix,
                          const std::string& values_placeholder, const std::vector<T>& items,
                          ParamExtractor param_extractor, size_t max_params_per_batch = 999)
    -> std::expected<std::vector<int64_t>, std::string> {
  if (items.empty()) {
    return std::vector<int64_t>{};
  }

  // 计算每个item需要的参数数量
  auto sample_params = param_extractor(items[0]);
  size_t params_per_item = sample_params.size();
  size_t max_items_per_batch = max_params_per_batch / params_per_item;

  if (max_items_per_batch == 0) {
    return std::unexpected("Single item exceeds maximum parameter limit");
  }

  std::vector<int64_t> all_inserted_ids;
  all_inserted_ids.reserve(items.size());

  return execute_transaction(
      state,
      [&](State::DatabaseState& db_state) -> std::expected<std::vector<int64_t>, std::string> {
        for (size_t batch_start = 0; batch_start < items.size();
             batch_start += max_items_per_batch) {
          size_t batch_end = std::min(batch_start + max_items_per_batch, items.size());
          size_t batch_size = batch_end - batch_start;

          // 构建当前批次的SQL
          std::string batch_sql = insert_prefix;
          std::vector<std::string> value_clauses;
          std::vector<Types::DbParam> all_params;

          value_clauses.reserve(batch_size);
          all_params.reserve(batch_size * params_per_item);

          for (size_t i = batch_start; i < batch_end; ++i) {
            value_clauses.push_back(values_placeholder);
            auto item_params = param_extractor(items[i]);
            all_params.insert(all_params.end(), item_params.begin(), item_params.end());
          }

          // 合并VALUES子句
          for (size_t i = 0; i < value_clauses.size(); ++i) {
            if (i > 0) batch_sql += ", ";
            batch_sql += value_clauses[i];
          }

          auto result = execute(db_state, batch_sql, all_params);
          if (!result) {
            return std::unexpected("Batch insert failed: " + result.error());
          }

          // 获取插入的ID范围
          auto last_id_result = query_scalar<int64_t>(db_state, "SELECT last_insert_rowid()");
          if (!last_id_result || !last_id_result->has_value()) {
            return std::unexpected("Failed to get last insert ID");
          }

          int64_t last_id = last_id_result->value();
          int64_t first_id = last_id - static_cast<int64_t>(batch_size) + 1;

          // 添加当前批次的ID到结果中
          for (int64_t id = first_id; id <= last_id; ++id) {
            all_inserted_ids.push_back(id);
          }
        }
        return all_inserted_ids;
      });
}

// 事务管理
export template <typename Func>
auto execute_transaction(State::DatabaseState& state, Func&& func) -> decltype(auto) {
  try {
    auto& connection = get_connection(state.db_path);
    SQLite::Transaction transaction(connection);

    // 执行用户提供的事务逻辑
    auto result = func(state);
    if (!result) {
      // 如果函数返回错误，事务会在transaction析构时自动回滚
      return result;
    }

    // 提交事务
    transaction.commit();
    return result;
  } catch (const SQLite::Exception& e) {
    // 异常发生时，transaction析构会自动回滚
    using ReturnType = std::decay_t<decltype(func(state))>;
    return ReturnType{std::unexpected("Transaction failed: " + std::string(e.what()))};
  } catch (const std::exception& e) {
    using ReturnType = std::decay_t<decltype(func(state))>;
    return ReturnType{std::unexpected("Transaction error: " + std::string(e.what()))};
  }
}

}  // namespace Core::Database