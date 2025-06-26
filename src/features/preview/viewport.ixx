module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

export module Features.Preview.Viewport;

import std;
import Features.Preview.State;
import Core.State;

export namespace Features::Preview::Viewport {

// 更新视口矩形状态
auto update_viewport_rect(Core::State::AppState& state) -> void;

// 检查游戏窗口是否完全可见
auto check_game_window_visibility(Core::State::AppState& state) -> bool;

// 渲染视口框到屏幕
auto render_viewport_frame(Core::State::AppState& state, ID3D11DeviceContext* context,
                           const Microsoft::WRL::ComPtr<ID3D11VertexShader>& vertex_shader,
                           const Microsoft::WRL::ComPtr<ID3D11PixelShader>& pixel_shader,
                           const Microsoft::WRL::ComPtr<ID3D11InputLayout>& input_layout) -> void;

// 创建视口框顶点数据
auto create_viewport_vertices(const Core::State::AppState& state,
                              std::vector<Features::Preview::State::ViewportVertex>& vertices)
    -> void;

// 计算视口框在预览窗口中的位置
auto calculate_viewport_position(const Core::State::AppState& state) -> RECT;

// 获取游戏窗口在屏幕上的位置
auto get_game_window_screen_rect(const Core::State::AppState& state) -> RECT;

// 计算游戏窗口相对屏幕的可见区域
auto calculate_visible_game_area(const Core::State::AppState& state) -> RECT;

}  // namespace Features::Preview::Viewport