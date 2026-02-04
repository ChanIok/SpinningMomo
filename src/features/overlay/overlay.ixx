module;

export module Features.Overlay;

import std;
import Core.State;
import <windows.h>;

namespace Features::Overlay {

// 开始叠加层捕捉
// freeze_after_first_frame: 首帧渲染后自动冻结（用于窗口变换场景）
export auto start_overlay(Core::State::AppState& state, HWND target_window,
                          bool freeze_after_first_frame = false)
    -> std::expected<void, std::string>;

// 停止叠加层
export auto stop_overlay(Core::State::AppState& state) -> void;

// 冻结叠加层（保持当前帧，停止处理新帧）
export auto freeze_overlay(Core::State::AppState& state) -> void;

// 解冻叠加层（恢复处理新帧）
export auto unfreeze_overlay(Core::State::AppState& state) -> void;

// 设置黑边模式
export auto set_letterbox_mode(Core::State::AppState& state, bool enabled) -> void;

// 清理资源
export auto cleanup_overlay(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay
