#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"

namespace features::overlay {

// 开始叠加层捕捉
// freeze_after_first_frame: 首帧渲染后自动冻结（用于窗口变换场景）
auto start_overlay(core::AppState& state, HWND target_window, bool freeze_after_first_frame = false)
    -> std::expected<void, std::string>;

// 停止叠加层。
// restore_target_window 为 true 时，按“正常关闭”处理；
// 为 false 时，按“目标窗口已失效”处理，不再尝试恢复目标窗口。
auto stop_overlay(core::AppState& state, bool restore_target_window = true) -> void;

// 冻结叠加层（保持当前帧，停止处理新帧）
auto freeze_overlay(core::AppState& state) -> void;

// 解冻叠加层（恢复处理新帧）
auto unfreeze_overlay(core::AppState& state) -> void;

// 设置黑边模式
auto set_letterbox_mode(core::AppState& state, bool enabled) -> void;

// 清理资源
auto cleanup_overlay(core::AppState& state) -> void;

}  // namespace features::overlay
