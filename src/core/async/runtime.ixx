module;

#include <asio.hpp>

export module Core.Async.Runtime;

import std;
import Core.Async.State;

namespace Core::Async {

// 启动异步运行时（包含初始化）
export auto start(Core::Async::State::AsyncRuntimeState& runtime, size_t thread_count = 0)
    -> std::expected<void, std::string>;

// 停止异步运行时（包含清理）
export auto stop(Core::Async::State::AsyncRuntimeState& runtime) -> void;

// 检查运行时是否正在运行
export auto is_running(const Core::Async::State::AsyncRuntimeState& runtime) -> bool;

// 获取io_context用于提交任务
export auto get_io_context(Core::Async::State::AsyncRuntimeState& runtime) -> asio::io_context*;

}  // namespace Core::Async