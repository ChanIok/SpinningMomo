module;

export module Features.ReplayBuffer;

import std;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.State;
import <windows.h>;

export namespace Features::ReplayBuffer {

// 初始化回放缓冲模块
auto initialize(Features::ReplayBuffer::State::ReplayBufferState& state)
    -> std::expected<void, std::string>;

// 开始缓冲
auto start_buffering(Features::ReplayBuffer::State::ReplayBufferState& state, HWND target_window,
                     const Features::ReplayBuffer::Types::ReplayBufferConfig& config)
    -> std::expected<void, std::string>;

// 停止缓冲
auto stop_buffering(Features::ReplayBuffer::State::ReplayBufferState& state) -> void;

// 强制轮转当前段落（截图时调用，获取最新视频数据）
auto force_rotate(Features::ReplayBuffer::State::ReplayBufferState& state)
    -> std::expected<void, std::string>;

// 获取最近的已完成段落路径（用于 Motion Photo / Replay）
auto get_recent_segments(const Features::ReplayBuffer::State::ReplayBufferState& state,
                         double duration_seconds) -> std::vector<std::filesystem::path>;

// 清理资源
auto cleanup(Features::ReplayBuffer::State::ReplayBufferState& state) -> void;

}  // namespace Features::ReplayBuffer
