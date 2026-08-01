#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/state/app_state.hpp"

namespace core::async {

// 启动异步运行时（包含初始化）
auto start(core::AppState& state, size_t thread_count = 0) -> std::expected<void, std::string>;

// 停止异步运行时（包含清理）
auto stop(core::AppState& state) -> void;

// 检查运行时是否正在运行
auto is_running(const core::AppState& state) -> bool;

// 获取io_context用于提交任务
auto get_io_context(core::AppState& state) -> asio::io_context*;

// 记录 fire-and-forget 协程的未处理异常。
auto log_exception(std::string_view operation, std::exception_ptr error) noexcept -> void;

inline auto log_completion(std::string operation) {
  return [operation = std::move(operation)](std::exception_ptr error) {
    log_exception(operation, error);
  };
}

}  // namespace core::async
