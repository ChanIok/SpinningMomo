module;

export module Features.ReplayBuffer;

import std;
import Features.ReplayBuffer.Types;
import Features.ReplayBuffer.State;
import Features.ReplayBuffer.DiskRingBuffer;
import Features.ReplayBuffer.Muxer;
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

// 获取最近 N 秒的帧元数据（从关键帧开始）
auto get_recent_frames(const Features::ReplayBuffer::State::ReplayBufferState& state,
                       double duration_seconds)
    -> std::expected<std::vector<DiskRingBuffer::FrameMetadata>, std::string>;

// 保存最近 N 秒到 MP4 文件
auto save_replay(Features::ReplayBuffer::State::ReplayBufferState& state, double duration_seconds,
                 const std::filesystem::path& output_path) -> std::expected<void, std::string>;

// 清理资源
auto cleanup(Features::ReplayBuffer::State::ReplayBufferState& state) -> void;

}  // namespace Features::ReplayBuffer
