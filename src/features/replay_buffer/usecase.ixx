module;

export module Features.ReplayBuffer.UseCase;

import std;
import Core.State;

namespace Features::ReplayBuffer::UseCase {

// === 功能开关 ===

// 切换动态照片模式（仅运行时状态）
export auto toggle_motion_photo(Core::State::AppState& state) -> std::expected<void, std::string>;

// 切换即时回放模式（仅运行时状态）
export auto toggle_replay_buffer(Core::State::AppState& state) -> std::expected<void, std::string>;

// === 保存操作 ===

// 保存 Motion Photo（截图完成后调用）
// jpeg_path: 已保存的 JPEG 截图路径
// 返回: Motion Photo 文件路径
export auto save_motion_photo(Core::State::AppState& state, const std::filesystem::path& jpeg_path)
    -> std::expected<std::filesystem::path, std::string>;

// 保存即时回放
// 返回: 回放视频文件路径
export auto save_replay(Core::State::AppState& state)
    -> std::expected<std::filesystem::path, std::string>;

// === 内部接口（用于 Initializer 自动恢复）===

// 确保后台录制已启动（当任一功能需要时调用）
export auto ensure_buffering_started(Core::State::AppState& state)
    -> std::expected<void, std::string>;

// 确保后台录制已停止（当两个功能都关闭时调用）
export auto ensure_buffering_stopped(Core::State::AppState& state) -> void;

}  // namespace Features::ReplayBuffer::UseCase
