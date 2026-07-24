#include "core/dialog_service/dialog_service.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"

#include "core/dialog_service/state.hpp"
#include "core/state/app_state.hpp"
#include "utils/dialog/dialog.hpp"
#include "utils/logger/logger.hpp"

namespace core::dialog_service {

namespace detail {

auto run_worker_loop(core::dialog_service::DialogServiceState& service, std::stop_token stop_token)
    -> void {
  auto com_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  while (!stop_token.stop_requested()) {
    std::move_only_function<void()> task;

    {
      std::unique_lock lock(service.queue_mutex);
      service.condition.wait(lock, [&service, &stop_token] {
        return stop_token.stop_requested() || service.shutdown_requested.load() ||
               !service.task_queue.empty();
      });

      if ((stop_token.stop_requested() || service.shutdown_requested.load()) &&
          service.task_queue.empty()) {
        break;
      }

      if (!service.task_queue.empty()) {
        task = std::move(service.task_queue.front());
        service.task_queue.pop();
      }
    }

    if (!task) {
      continue;
    }

    try {
      task();
    } catch (const std::exception& e) {
      Logger().error("DialogService task execution error: {}", e.what());
    } catch (...) {
      Logger().error("DialogService task execution unknown error");
    }
  }
}

template <typename Result>
using DialogResult = std::expected<Result, std::string>;

template <typename Result, typename Task>
  requires std::invocable<Task&> && std::same_as<std::invoke_result_t<Task&>, DialogResult<Result>>
auto submit_dialog_task(core::dialog_service::DialogServiceState& service, Task&& task)
    -> DialogResult<Result> {
  if (!service.is_running.load()) {
    return std::unexpected("DialogService is not running");
  }

  if (service.shutdown_requested.load()) {
    return std::unexpected("DialogService is shutting down");
  }

  std::promise<DialogResult<Result>> promise;
  auto future = promise.get_future();

  try {
    {
      std::lock_guard lock(service.queue_mutex);
      service.task_queue.push(
          [task = std::forward<Task>(task), promise = std::move(promise)]() mutable {
            try {
              promise.set_value(std::invoke(task));
            } catch (const std::exception& e) {
              promise.set_value(std::unexpected(std::string("Dialog task failed: ") + e.what()));
            } catch (...) {
              promise.set_value(std::unexpected("Dialog task failed: unknown error"));
            }
          });
    }

    service.condition.notify_one();
    return future.get();
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Failed to submit dialog task: ") + e.what());
  }
}

}  // namespace detail

auto start(core::AppState& state) -> std::expected<void, std::string> {
  if (!state.dialog_service) {
    return std::unexpected("DialogServiceState is not initialized");
  }
  auto& service = *state.dialog_service;

  if (service.is_running.exchange(true)) {
    Logger().warn("DialogService already started");
    return std::unexpected("DialogService already started");
  }

  try {
    service.shutdown_requested = false;
    service.worker_thread = std::jthread(
        [&service](std::stop_token stop_token) { detail::run_worker_loop(service, stop_token); });

    Logger().info("DialogService started successfully");
    return {};
  } catch (const std::exception& e) {
    service.is_running = false;
    service.shutdown_requested = false;
    return std::unexpected(std::string("Failed to start DialogService: ") + e.what());
  }
}

auto stop(core::AppState& state) -> void {
  if (!state.dialog_service) {
    return;
  }
  auto& service = *state.dialog_service;
  if (!service.is_running.exchange(false)) {
    return;
  }

  Logger().info("Stopping DialogService");

  service.shutdown_requested = true;
  service.condition.notify_all();

  if (service.worker_thread.joinable()) {
    service.worker_thread.request_stop();
    service.worker_thread.join();
  }

  {
    std::lock_guard lock(service.queue_mutex);
    std::queue<std::move_only_function<void()>> empty;
    service.task_queue.swap(empty);
  }

  service.shutdown_requested = false;
  Logger().info("DialogService stopped");
}

auto open_file(core::AppState& state, const utils::dialog::FileSelectorParams& params, HWND hwnd)
    -> std::expected<utils::dialog::FileSelectorResult, std::string> {
  if (!state.dialog_service) {
    return std::unexpected("DialogService state is not initialized");
  }

  return detail::submit_dialog_task<utils::dialog::FileSelectorResult>(
      *state.dialog_service, [params, hwnd]() { return utils::dialog::select_file(params, hwnd); });
}

auto open_folder(core::AppState& state, const utils::dialog::FolderSelectorParams& params,
                 HWND hwnd) -> std::expected<utils::dialog::FolderSelectorResult, std::string> {
  if (!state.dialog_service) {
    return std::unexpected("DialogService state is not initialized");
  }

  return detail::submit_dialog_task<utils::dialog::FolderSelectorResult>(
      *state.dialog_service,
      [params, hwnd]() { return utils::dialog::select_folder(params, hwnd); });
}

}  // namespace core::dialog_service
