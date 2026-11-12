#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "utils/display/display.hpp"

namespace features::preview {

// 在应用 UI 线程使用调用方确定的游戏视口屏幕开始捕获并显示预览。
auto start_preview(core::AppState& state, HWND target_window,
                   const utils::display::MonitorInfo& monitor_info)
    -> std::expected<void, std::string>;

// 更新游戏视口的基准屏幕，不移动预览窗自身。
auto set_working_monitor(core::AppState& state, const utils::display::MonitorInfo& monitor) -> void;

// 停止预览并隐藏窗口
auto stop_preview(core::AppState& state) -> void;

// 在 UI 线程结束失败会话并通知用户。
auto fail_preview(core::AppState& state, const std::string& error) -> void;

// DPI 处理
auto update_preview_dpi(core::AppState& state, UINT new_dpi) -> void;

// 清理资源
auto cleanup_preview(core::AppState& state) -> void;

}  // namespace features::preview
