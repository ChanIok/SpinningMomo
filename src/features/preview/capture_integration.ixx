module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

export module Features.Preview.CaptureIntegration;

import std;
import Features.Preview.State;
import Core.State;
import Utils.Graphics.Capture;

export namespace Features::Preview::CaptureIntegration {

// 类型别名，使用Types中统一定义的CaptureSession
using CaptureSession = Features::Preview::State::CaptureSession;

// 初始化捕获系统
auto initialize_capture(Core::State::AppState& state, HWND target_window, int width, int height)
    -> std::expected<void, std::string>;

// 开始捕获
auto start_capture(Core::State::AppState& state) -> std::expected<void, std::string>;

// 停止捕获
auto stop_capture(Core::State::AppState& state) -> void;

// 清理捕获资源
auto cleanup_capture(Core::State::AppState& state) -> void;

// 帧到达回调处理
auto on_frame_arrived(Core::State::AppState& state, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    -> void;

// 检查捕获是否正在运行
auto is_capture_active(const Core::State::AppState& state) -> bool;

// 获取捕获会话（从状态中）
auto get_capture_session(Core::State::AppState& state) -> CaptureSession*;

}  // namespace Features::Preview::CaptureIntegration