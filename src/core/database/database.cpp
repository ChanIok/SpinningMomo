module;

module Core.Database;

import std;
import Core.Database.State;
import Core.Database.Types;
import Utils.Logger;
import <SQLiteCpp/SQLiteCpp.h>;

namespace Core::Database {

namespace Executor {

auto resolve_thread_count() -> std::size_t {
  const auto hardware_threads = std::thread::hardware_concurrency();
  return hardware_threads >= 8 ? 4 : 2;
}

auto configure_connection(SQLite::Database& connection) -> void {
  // 每个 DB worker 各持有一个连接，连接级 PRAGMA 必须逐个设置。
  connection.exec("PRAGMA busy_timeout=5000;");
  connection.exec("PRAGMA journal_mode=WAL;");
  connection.exec("PRAGMA synchronous=NORMAL;");
  connection.exec("PRAGMA foreign_keys=ON;");
}

auto validate_database_path(const std::filesystem::path& db_path) -> void {
  SQLite::Database connection(db_path.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
  configure_connection(connection);
}

auto bind_params(SQLite::Statement& query, const std::vector<Types::DbParam>& params) -> void {
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
}

auto worker_loop(State::DatabaseState& state, std::size_t index) -> void {
  try {
    SQLite::Database connection(state.db_path.string(),
                                SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    configure_connection(connection);

    // 标记当前线程为 DB worker；事务内部重入会直接复用这个连接。
    current_connection = &connection;

    Logger().info("Database worker {} started", index);

    while (true) {
      std::function<void()> task;
      {
        std::unique_lock lock(state.queue_mutex);
        state.queue_cv.wait(lock, [&state] {
          return state.shutdown_requested.load(std::memory_order_acquire) ||
                 !state.task_queue.empty();
        });

        if (state.task_queue.empty() && state.shutdown_requested.load(std::memory_order_acquire)) {
          break;
        }

        if (!state.task_queue.empty()) {
          task = std::move(state.task_queue.front());
          state.task_queue.pop();
        }
      }

      if (task) {
        try {
          task();
        } catch (const std::exception& e) {
          Logger().error("Database worker {} task error: {}", index, e.what());
        } catch (...) {
          Logger().error("Database worker {} task error: unknown", index);
        }
      }
    }

    current_connection = nullptr;
    Logger().info("Database worker {} stopped", index);
  } catch (const SQLite::Exception& e) {
    current_connection = nullptr;
    Logger().error("Database worker {} SQLite error: {}", index, e.what());
  } catch (const std::exception& e) {
    current_connection = nullptr;
    Logger().error("Database worker {} error: {}", index, e.what());
  }
}

}  // namespace Executor

auto submit_task(State::DatabaseState& state, std::function<void()> task)
    -> std::expected<void, std::string> {
  if (!state.is_running.load(std::memory_order_acquire)) {
    return std::unexpected("Database executor is not running");
  }
  if (state.shutdown_requested.load(std::memory_order_acquire)) {
    return std::unexpected("Database executor is shutting down");
  }
  try {
    {
      std::lock_guard lock(state.queue_mutex);
      if (state.shutdown_requested.load(std::memory_order_acquire)) {
        return std::unexpected("Database executor is shutting down");
      }
      state.task_queue.push(std::move(task));
    }
    // 共享队列由所有 DB worker 竞争消费；不再维护调用线程到 worker 的亲和关系。
    state.queue_cv.notify_one();
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to submit database task: " + std::string(e.what()));
  }
}

auto initialize(State::DatabaseState& state, const std::filesystem::path& db_path)
    -> std::expected<void, std::string> {
  if (state.is_running.exchange(true, std::memory_order_acq_rel)) {
    Logger().warn("Database executor already started");
    return std::unexpected("Database executor already started");
  }

  try {
    state.db_path = db_path;
    state.shutdown_requested.store(false, std::memory_order_release);

    if (db_path.has_parent_path()) {
      std::filesystem::create_directories(db_path.parent_path());
    }

    // 先用短连接验证路径和基础 PRAGMA，避免 worker 启动后才暴露打开失败。
    Executor::validate_database_path(db_path);

    state.thread_count = Executor::resolve_thread_count();
    state.worker_threads.clear();
    state.worker_threads.reserve(state.thread_count);

    for (std::size_t i = 0; i < state.thread_count; ++i) {
      state.worker_threads.emplace_back([&state, i]() { Executor::worker_loop(state, i); });
    }

    Logger().info("Database executor started with {} worker(s): {}", state.thread_count,
                  db_path.string());
    return {};
  } catch (const SQLite::Exception& e) {
    Logger().error("Cannot initialize database: {} - Error: {}", db_path.string(), e.what());
    shutdown(state);
    return std::unexpected(std::string("Cannot initialize database: ") + e.what());
  } catch (const std::exception& e) {
    Logger().error("Cannot initialize database: {} - Error: {}", db_path.string(), e.what());
    shutdown(state);
    return std::unexpected(std::string("Cannot initialize database: ") + e.what());
  }
}

auto shutdown(State::DatabaseState& state) -> void {
  if (!state.is_running.exchange(false, std::memory_order_acq_rel)) {
    return;
  }

  Logger().info("Stopping database executor");
  state.shutdown_requested.store(true, std::memory_order_release);
  // 唤醒所有 worker；它们会先跑完队列中的任务，再在队列为空时退出。
  state.queue_cv.notify_all();

  for (auto& worker_thread : state.worker_threads) {
    if (worker_thread.joinable()) {
      worker_thread.join();
    }
  }

  {
    std::lock_guard lock(state.queue_mutex);
    std::queue<std::function<void()>> empty;
    state.task_queue.swap(empty);
  }

  state.worker_threads.clear();
  state.thread_count = 0;
  state.shutdown_requested.store(false, std::memory_order_release);

  Logger().info("Database executor stopped");
}

auto execute(State::DatabaseState& state, const std::string& sql)
    -> std::expected<void, std::string> {
  return run_on_database<std::expected<void, std::string>>(
      state, [sql](SQLite::Database& connection) -> std::expected<void, std::string> {
        try {
          connection.exec(sql);
          return {};
        } catch (const SQLite::Exception& e) {
          Logger().error("Failed to execute statement: {} - Error: {}", sql, e.what());
          return std::unexpected(std::string("Failed to execute statement: ") + e.what());
        }
      });
}

auto execute(State::DatabaseState& state, const std::string& sql,
             const std::vector<Types::DbParam>& params) -> std::expected<void, std::string> {
  return run_on_database<std::expected<void, std::string>>(
      state, [sql, params](SQLite::Database& connection) -> std::expected<void, std::string> {
        try {
          SQLite::Statement query(connection, sql);
          Executor::bind_params(query, params);
          query.exec();
          return {};
        } catch (const SQLite::Exception& e) {
          Logger().error("Failed to execute statement: {} - Error: {}", sql, e.what());
          return std::unexpected(std::string("Failed to execute statement: ") + e.what());
        }
      });
}

}  // namespace Core::Database
