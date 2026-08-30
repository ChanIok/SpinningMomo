#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/adb_mode/types.hpp"

namespace features::adb_mode {

// 启动 ADB 专用串行线程。
auto initialize(core::AppState& state) -> std::expected<void, std::string>;

// 在 UI 和通知服务就绪后启动自动连接任务。
auto schedule_startup_tasks(core::AppState& state) -> void;

// 停止接收任务，恢复物理尺寸并等待 ADB 专用线程退出。
auto shutdown(core::AppState& state) -> void;

// 合并设置和运行时状态，生成可安全发送给前端的状态快照。
auto get_status(const core::AppState& state) -> AdbModeStatus;
// 只检查当前是否存在可以执行显示操作的已连接设备。
auto is_connected(const core::AppState& state) -> bool;

// 将截图任务放入 ADB 专用队列，并在完成后通过回调返回结果。
auto capture_screen_async(
    core::AppState& state, const std::filesystem::path& output_path,
    std::move_only_function<void(bool success, const std::wstring& path)> completion_callback)
    -> bool;

// 将恢复物理尺寸任务放入队列，并同步浮窗的 Default 菜单状态。
auto restore_async(core::AppState& state) -> bool;
// 将连接或恢复并断开任务放入队列。
auto toggle_async(core::AppState& state) -> bool;

// 连接配置变化时重新建立当前活动连接；关闭 ADB 模式必须先恢复显示状态。
auto handle_settings_changed(core::AppState& state) -> void;

// 按当前分辨率预设重新计算并异步应用新的宽高比。
auto handle_ratio_changed(core::AppState& state, std::size_t ratio_index, double ratio_value)
    -> void;
// 按当前宽高比重新计算并异步应用新的分辨率预设。
auto handle_resolution_changed(core::AppState& state, std::size_t resolution_index) -> void;

}  // namespace features::adb_mode
