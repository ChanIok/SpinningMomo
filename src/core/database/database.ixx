module;

export module Core.Database;

import std;
import Core.Database.State;
import Core.Database.Types;
import Core.Database.DataMapper;
import <SQLiteCpp/SQLiteCpp.h>;

namespace Core::Database {

// DB worker 线程内的当前连接。业务线程只投递任务，不直接持有 SQLite 连接。
// 事务 lambda 内再次调用 execute/query 时会命中它并直接执行，避免任务重入死锁。
thread_local SQLite::Database* current_connection = nullptr;

auto submit_task(State::DatabaseState& state, std::function<void()> task)
    -> std::expected<void, std::string>;

// 初始化数据库连接
export auto initialize(State::DatabaseState& state, const std::filesystem::path& db_path)
    -> std::expected<void, std::string>;

export auto shutdown(State::DatabaseState& state) -> void;

// 用于 `... RETURNING id` 的通用单列行类型。多行返回时用 query<ReturningIdRow>() 计数。
export struct ReturningIdRow {
  std::int64_t id = 0;
};

template <typename Result>
auto make_database_error(std::string error) -> Result {
  return Result{std::unexpected(std::move(error))};
}

template <typename Result, typename Func>
auto run_on_database(State::DatabaseState& state, Func&& func) -> Result {
  // 已在 DB worker 内：直接使用当前连接。事务内部的所有 SQL 都靠这个分支保持在
  // 同一连接、同一任务中执行。
  if (current_connection) {
    try {
      return func(*current_connection);
    } catch (const SQLite::Exception& e) {
      return make_database_error<Result>("SQLite error: " + std::string(e.what()));
    } catch (const std::exception& e) {
      return make_database_error<Result>("Database task error: " + std::string(e.what()));
    }
  }

  if (!state.is_running.load(std::memory_order_acquire)) {
    return make_database_error<Result>("Database executor is not running");
  }

  // 对外保持同步 API：调用线程等待 promise，实际 SQLite 操作在 DB worker 线程执行。
  auto promise = std::make_shared<std::promise<Result>>();
  auto future = promise->get_future();
  auto task = [promise, fn = std::forward<Func>(func)]() mutable {
    if (!current_connection) {
      promise->set_value(make_database_error<Result>("Database worker connection is not ready"));
      return;
    }

    try {
      promise->set_value(fn(*current_connection));
    } catch (const SQLite::Exception& e) {
      promise->set_value(make_database_error<Result>("SQLite error: " + std::string(e.what())));
    } catch (const std::exception& e) {
      promise->set_value(
          make_database_error<Result>("Database task error: " + std::string(e.what())));
    } catch (...) {
      promise->set_value(make_database_error<Result>("Database task error: unknown"));
    }
  };

  if (auto submit_result = submit_task(state, std::move(task)); !submit_result) {
    return make_database_error<Result>(submit_result.error());
  }

  return future.get();
}

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
  return run_on_database<std::expected<std::vector<T>, std::string>>(
      state,
      [sql, params](SQLite::Database& connection) -> std::expected<std::vector<T>, std::string> {
        try {
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
                    query.bind(param_index, arg.data(),
                               static_cast<int>(arg.size()));  // 绑定 BLOB
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
      });
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
  return run_on_database<std::expected<std::optional<T>, std::string>>(
      state,
      [sql, params](SQLite::Database& connection) -> std::expected<std::optional<T>, std::string> {
        try {
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
      });
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

          batch_sql += " RETURNING id";
          auto inserted_ids_result = query<ReturningIdRow>(db_state, batch_sql, all_params);
          if (!inserted_ids_result) {
            return std::unexpected("Batch insert failed: " + inserted_ids_result.error());
          }

          // SQLite RETURNING 按实际插入行返回 ID，不依赖 last_insert_rowid 的连接状态。
          for (const auto& row : inserted_ids_result.value()) {
            all_inserted_ids.push_back(row.id);
          }
        }
        return all_inserted_ids;
      });
}

// 事务管理：整个 lambda 会作为一个 DB task 执行；lambda 内的 execute/query 不会重新入队。
export template <typename Func>
auto execute_transaction(State::DatabaseState& state, Func&& func) -> decltype(auto) {
  using ReturnType = std::decay_t<decltype(func(state))>;
  return run_on_database<ReturnType>(
      state,
      [&state, fn = std::forward<Func>(func)](SQLite::Database& connection) mutable -> ReturnType {
        try {
          SQLite::Transaction transaction(connection);

          // 执行用户提供的事务逻辑。事务内的 execute/query 会检测当前 DB 线程并直接执行。
          auto result = fn(state);
          if (!result) {
            // 如果函数返回错误，事务会在 transaction 析构时自动回滚。
            return result;
          }

          // 提交事务
          transaction.commit();
          return result;
        } catch (const SQLite::Exception& e) {
          // 异常发生时，transaction 析构会自动回滚。
          return ReturnType{std::unexpected("Transaction failed: " + std::string(e.what()))};
        } catch (const std::exception& e) {
          return ReturnType{std::unexpected("Transaction error: " + std::string(e.what()))};
        }
      });
}

}  // namespace Core::Database
