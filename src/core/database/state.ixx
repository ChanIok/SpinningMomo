module;

export module Core.Database.State;

import std;

export namespace Core::Database::State {

struct DatabaseState {
  // 存储数据库文件路径
  std::filesystem::path db_path;

  std::queue<std::function<void()>> task_queue;
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::vector<std::jthread> worker_threads;
  std::atomic<bool> is_running{false};
  std::atomic<bool> shutdown_requested{false};
  std::size_t thread_count = 0;
};

}  // namespace Core::Database::State
