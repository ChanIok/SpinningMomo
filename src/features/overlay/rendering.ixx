module;

#include <d3d11.h>
#include <wrl/client.h>

export module Features.Overlay.Rendering;

import std;
import Core.State;
import Features.Overlay.State;

namespace Features::Overlay::Rendering {

// 初始化渲染系统
export auto initialize_rendering(Core::State::AppState& state)
    -> std::expected<void, std::string>;

// 调整交换链大小
export auto resize_rendering(Core::State::AppState& state) -> std::expected<void, std::string>;

// 渲染帧
export auto render_frame(Core::State::AppState& state,
                         Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) -> void;

// 清理渲染资源
export auto cleanup_rendering_resources(Core::State::AppState& state) -> void;

// 检查是否有新帧
export auto has_new_frame(const Core::State::AppState& state) -> bool;

}  // namespace Features::Overlay::Rendering
