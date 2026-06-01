module;

export module Features.Recording.Session;

import std;
import Core.State;
import Features.Recording.Types;
import <windows.h>;

namespace Features::Recording::Session {

// 根据 WGC 源帧尺寸计算本段录制的输出尺寸和裁剪区域。
export auto resolve_capture_plan(HWND target_window, bool capture_client_area, int frame_width,
                                 int frame_height)
    -> std::expected<Features::Recording::Types::CapturePlan, std::string>;

// 正常录制中刷新裁剪计划，并校验输出尺寸仍匹配当前编码器配置。
export auto calculate_frame_crop_plan(HWND target_window,
                                      const Features::Recording::Types::RecordingConfig& config,
                                      int frame_width, int frame_height)
    -> std::expected<Features::Recording::Types::CapturePlan, std::string>;

// 启动录制前读取 WGC 真实捕获尺寸，并生成初始捕获计划。
export auto build_startup_capture_plan(HWND target_window, bool capture_client_area)
    -> std::expected<Features::Recording::Types::CapturePlan, std::string>;

// 生成无扩展名的临时输出路径，避免未 finalize 的文件被当成完整 MP4。
export auto build_working_output_path(const std::filesystem::path& final_output_path)
    -> std::filesystem::path;

// 在指定输出目录里生成一个时间戳 MP4 路径；首次录制和 resize 自动切段共用这套命名规则。
export auto build_output_path_in_directory(const std::filesystem::path& output_directory)
    -> std::filesystem::path;

// finalize 成功后把临时文件发布为最终 .mp4 文件。
export auto rename_working_output_to_final(const std::filesystem::path& working_output_path,
                                           const std::filesystem::path& final_output_path)
    -> std::expected<void, std::string>;

// 删除失败或过短录制段留下的临时输出文件。
export auto delete_working_output_file(const std::filesystem::path& working_output_path,
                                       std::string_view reason) -> void;

// 清空单段录制会话状态，但保留可复用的 D3D 设备资源。
export auto clear_session_runtime_fields(Core::State::AppState& app_state) -> void;

// 取消延迟释放 D3D 资源的定时器。
export auto cancel_cleanup_timer(Core::State::AppState& app_state) -> void;

// 启动延迟释放 D3D 资源的定时器，高频启停时避免反复初始化设备。
export auto start_cleanup_timer(Core::State::AppState& app_state, std::function<void()> on_timeout)
    -> void;

// 立即释放录制模块可复用的 D3D / WinRT 设备资源。
export auto cleanup_d3d_resources(Core::State::AppState& app_state) -> void;

// 确保录制所需的 D3D device/context 和 WinRT device 已创建。
export auto ensure_d3d_resources_ready(Core::State::AppState& app_state)
    -> std::expected<void, std::string>;

// 停止 WGC 继续产帧，但保留 frame pool 供编码线程排空。
export auto stop_capture_input(Core::State::AppState& app_state) -> void;

// 清理当前 WGC 捕获会话资源。
export auto cleanup_capture_session(Core::State::AppState& app_state) -> void;

}  // namespace Features::Recording::Session
