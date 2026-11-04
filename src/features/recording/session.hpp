#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "features/recording/types.hpp"

namespace features::recording::session {

// 根据 WGC 源帧尺寸计算本段录制的输出尺寸和裁剪区域。
auto resolve_capture_plan(HWND target_window, bool capture_client_area, int frame_width,
                          int frame_height)
    -> std::expected<features::recording::CapturePlan, std::string>;

// 正常录制中刷新裁剪计划，并校验输出尺寸仍匹配当前编码器配置。
auto calculate_frame_crop_plan(HWND target_window,
                               const features::recording::RecordingConfig& config, int frame_width,
                               int frame_height)
    -> std::expected<features::recording::CapturePlan, std::string>;

// 启动录制前读取 WGC 真实捕获尺寸，并生成初始捕获计划。
auto build_startup_capture_plan(HWND target_window, bool capture_client_area)
    -> std::expected<features::recording::CapturePlan, std::string>;

// 在指定输出目录里生成一个时间戳 MP4 路径；首次录制和 resize 自动切段共用这套命名规则。
auto build_output_path_in_directory(const std::filesystem::path& output_directory)
    -> std::filesystem::path;

// 仅删除启动失败且没有有效输出的文件，或沿用误触短段丢弃规则。
auto delete_output_file(const std::filesystem::path& output_path, std::string_view reason) -> void;

// 清空单段录制会话状态，但保留可复用的 D3D 设备资源。
auto clear_session_runtime_fields(core::AppState& app_state) -> void;

// 取消延迟释放 D3D 资源的定时器。
auto cancel_cleanup_timer(core::AppState& app_state) -> void;

// 启动延迟释放 D3D 资源的定时器，高频启停时避免反复初始化设备。
auto start_cleanup_timer(core::AppState& app_state, std::move_only_function<void()> on_timeout)
    -> void;

// 立即释放录制模块可复用的 D3D / WinRT 设备资源。
auto cleanup_d3d_resources(core::AppState& app_state) -> void;

// 确保录制所需的 D3D device/context 和 WinRT device 已创建。
auto ensure_d3d_resources_ready(core::AppState& app_state) -> std::expected<void, std::string>;

// 停止 WGC 继续产帧，但保留 frame pool 供编码线程排空。
auto stop_capture_input(core::AppState& app_state) -> void;

// 清理当前 WGC 捕获会话资源。
auto cleanup_capture_session(core::AppState& app_state) -> void;

}  // namespace features::recording::session
