module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

export module Features.Overlay.Capture;

import std;
import Core.State;
import Features.Overlay.State;

namespace Features::Overlay::Capture {

// 初始化捕获系统
export auto initialize_capture(Core::State::AppState& state) -> std::expected<void, std::string>;

// 开始捕获指定窗口
export auto start_capture(Core::State::AppState& state, HWND target_window)
    -> std::expected<void, std::string>;

// 停止捕获
export auto stop_capture(Core::State::AppState& state) -> void;

// 清理捕获资源
export auto cleanup_capture(Core::State::AppState& state) -> void;

// 检查是否正在捕获
export auto is_capturing(const Core::State::AppState& state) -> bool;

// 检查系统是否支持捕获
export auto is_capture_supported() -> bool;

// 创建WinRT设备
export auto create_winrt_device(Core::State::AppState& state) -> std::expected<void, std::string>;

// 帧到达回调处理
export auto handle_frame_arrived(Core::State::AppState& state,
                                 Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) -> void;

}  // namespace Features::Overlay::Capture
