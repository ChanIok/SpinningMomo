module;

export module Features.Overlay.Threads;

import std;
import Core.State;
import Features.Overlay.State;

namespace Features::Overlay::Threads {

// 启动所有工作线程
export auto start_threads(Core::State::AppState& state) -> std::expected<void, std::string>;

// 停止所有线程
export auto stop_threads(Core::State::AppState& state) -> void;

// 等待所有线程结束
export auto wait_for_threads(Core::State::AppState& state) -> void;

// 捕获和渲染线程处理函数
export auto capture_render_thread_proc(Core::State::AppState& state, std::stop_token token) -> void;

// 钩子线程处理函数
export auto hook_thread_proc(Core::State::AppState& state, std::stop_token token) -> void;

// 窗口管理线程处理函数
export auto window_manager_thread_proc(Core::State::AppState& state, std::stop_token token) -> void;

// 检查线程是否正在运行
export auto are_threads_running(const Core::State::AppState& state) -> bool;

}  // namespace Features::Overlay::Threads
