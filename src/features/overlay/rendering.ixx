module;

#include <d3d11.h>
#include <wrl/client.h>

export module Features.Overlay.Rendering;

import std;
import Core.State;
import Features.Overlay.State;

namespace Features::Overlay::Rendering {

// 初始化D3D渲染资源
export auto initialize_d3d_rendering(Core::State::AppState& state)
    -> std::expected<void, std::string>;

// 初始化渲染状态
export auto initialize_render_states(Core::State::AppState& state)
    -> std::expected<void, std::string>;

// 调整交换链大小
export auto resize_swap_chain(Core::State::AppState& state) -> std::expected<void, std::string>;

// 创建渲染目标
export auto create_render_target(Core::State::AppState& state) -> std::expected<void, std::string>;

// 创建着色器资源
export auto create_shader_resources(Core::State::AppState& state)
    -> std::expected<void, std::string>;

// 执行渲染
export auto perform_rendering(Core::State::AppState& state) -> void;

// 处理新帧到达
export auto on_frame_arrived(Core::State::AppState& state,
                             Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture) -> void;

// 更新纹理资源
export auto update_texture_resources(Core::State::AppState& state,
                                     Microsoft::WRL::ComPtr<ID3D11Texture2D> frame_texture)
    -> std::expected<void, std::string>;

// 清理渲染资源
export auto cleanup_rendering_resources(Core::State::AppState& state) -> void;

// 检查是否有新帧
export auto has_new_frame(const Core::State::AppState& state) -> bool;

// 设置新帧标志
export auto set_new_frame_flag(Core::State::AppState& state, bool has_frame) -> void;

}  // namespace Features::Overlay::Rendering
