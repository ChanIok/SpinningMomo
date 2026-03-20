module;

#include <wil/com.h>

module Core.DialogService;

import std;
import Core.DialogService.State;
import Core.State;
import Utils.Dialog;
import Utils.Logger;

namespace Core::DialogService {

namespace Detail {

auto run_worker_loop(Core::DialogService::State::DialogServiceState& service,
                     std::stop_token stop_token) -> void {
  auto com_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  while (!stop_token.stop_requested()) {
    std::function<void()> task;

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

template <typename Result>
auto submit_dialog_task(Core::DialogService::State::DialogServiceState& service,
                        std::function<DialogResult<Result>()> task) -> DialogResult<Result> {
  if (!service.is_running.load()) {
    return std::unexpected("DialogService is not running");
  }

  if (service.shutdown_requested.load()) {
    return std::unexpected("DialogService is shutting down");
  }

  auto promise = std::make_shared<std::promise<DialogResult<Result>>>();
  auto future = promise->get_future();

  try {
    {
      std::lock_guard lock(service.queue_mutex);
      service.task_queue.push([task = std::move(task), promise]() mutable {
        try {
          promise->set_value(task());
        } catch (const std::exception& e) {
          promise->set_value(std::unexpected(std::string("Dialog task failed: ") + e.what()));
        } catch (...) {
          promise->set_value(std::unexpected("Dialog task failed: unknown error"));
        }
      });
    }

    service.condition.notify_one();
    return future.get();
  } catch (const std::exception& e) {
    return std::unexpected(std::string("Failed to submit dialog task: ") + e.what());
  }
}

}  // namespace Detail

auto start(Core::DialogService::State::DialogServiceState& service)
    -> std::expected<void, std::string> {
  if (service.is_running.exchange(true)) {
    Logger().warn("DialogService already started");
    return std::unexpected("DialogService already started");
  }

  try {
    service.shutdown_requested = false;
    service.worker_thread = std::jthread(
        [&service](std::stop_token stop_token) { Detail::run_worker_loop(service, stop_token); });

    Logger().info("DialogService started successfully");
    return {};
  } catch (const std::exception& e) {
    service.is_running = false;
    service.shutdown_requested = false;
    return std::unexpected(std::string("Failed to start DialogService: ") + e.what());
  }
}

auto stop(Core::DialogService::State::DialogServiceState& service) -> void {
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
    std::queue<std::function<void()>> empty;
    service.task_queue.swap(empty);
  }

  service.shutdown_requested = false;
  Logger().info("DialogService stopped");
}

auto open_file(Core::State::AppState& state, const Utils::Dialog::FileSelectorParams& params,
               HWND hwnd) -> std::expected<Utils::Dialog::FileSelectorResult, std::string> {
  if (!state.dialog_service) {
    return std::unexpected("DialogService state is not initialized");
  }

  return Detail::submit_dialog_task<Utils::Dialog::FileSelectorResult>(
      *state.dialog_service, [params, hwnd]() { return Utils::Dialog::select_file(params, hwnd); });
}

auto open_folder(Core::State::AppState& state, const Utils::Dialog::FolderSelectorParams& params,
                 HWND hwnd) -> std::expected<Utils::Dialog::FolderSelectorResult, std::string> {
  if (!state.dialog_service) {
    return std::unexpected("DialogService state is not initialized");
  }

  return Detail::submit_dialog_task<Utils::Dialog::FolderSelectorResult>(
      *state.dialog_service,
      [params, hwnd]() { return Utils::Dialog::select_folder(params, hwnd); });
}

}  // namespace Core::DialogService
