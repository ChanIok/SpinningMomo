module;

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <iostream>

module Features.Preview.Viewport;

import std;
import Features.Preview.State;
import Core.State;
import Utils.Graphics.D3D;
import Utils.Logger;
import Features.Preview.Rendering;

namespace Features::Preview::Viewport {

auto update_viewport_rect(Core::State::AppState& state) -> void {
  if (!state.preview.target_window || !state.preview.hwnd) {
    return;
  }

  // 更新游戏窗口位置信息
  state.preview.game_window_rect = get_game_window_screen_rect(state);

  // 检查游戏窗口是否完全可见
  state.preview.viewport.game_window_fully_visible = check_game_window_visibility(state);

  if (state.preview.viewport.game_window_fully_visible) {
    // 如果游戏窗口完全可见，隐藏视口框
    state.preview.viewport.visible = false;
    return;
  }

  // 游戏窗口超出屏幕，显示视口框
  state.preview.viewport.visible = true;
  state.preview.viewport.viewport_rect = calculate_viewport_position(state);
}

auto check_game_window_visibility(Core::State::AppState& state) -> bool {
  if (!state.preview.target_window) {
    return false;
  }

  RECT gameRect = get_game_window_screen_rect(state);

  // 获取屏幕尺寸
  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  // 检查游戏窗口是否完全在屏幕内
  return (gameRect.left >= 0 && gameRect.top >= 0 && gameRect.right <= screenWidth &&
          gameRect.bottom <= screenHeight);
}

auto render_viewport_frame(Core::State::AppState& state, ID3D11DeviceContext* context,
                           const Microsoft::WRL::ComPtr<ID3D11VertexShader>& vertex_shader,
                           const Microsoft::WRL::ComPtr<ID3D11PixelShader>& pixel_shader,
                           const Microsoft::WRL::ComPtr<ID3D11InputLayout>& input_layout) -> void {
  if (!state.preview.viewport.visible || !context) {
    return;
  }

  // 创建视口框顶点数据
  std::vector<Features::Preview::State::ViewportVertex> vertices;
  create_viewport_vertices(state, vertices);

  if (vertices.empty()) {
    return;
  }

  // 获取渲染资源
  auto* rendering_resources = Features::Preview::Rendering::get_rendering_resources(state);
  if (!rendering_resources->initialized) {
    Logger().error("Rendering resources not initialized");
    return;
  }

  // 创建动态顶点缓冲区
  auto buffer_result = Utils::Graphics::D3D::create_vertex_buffer(
      rendering_resources->d3d_context.device.Get(), vertices.data(), vertices.size(),
      sizeof(Features::Preview::State::ViewportVertex),
      true);  // 动态缓冲区

  if (!buffer_result) {
    Logger().error("Failed to create viewport vertex buffer");
    return;
  }

  auto viewport_buffer = buffer_result.value();

  // 设置渲染状态
  context->IASetInputLayout(input_layout.Get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

  UINT stride = sizeof(Features::Preview::State::ViewportVertex);
  UINT offset = 0;
  context->IASetVertexBuffers(0, 1, viewport_buffer.GetAddressOf(), &stride, &offset);

  context->VSSetShader(vertex_shader.Get(), nullptr, 0);
  context->PSSetShader(pixel_shader.Get(), nullptr, 0);

  // 绘制视口框线条
  context->Draw(static_cast<UINT>(vertices.size()), 0);
}

auto create_viewport_vertices(const Core::State::AppState& state,
                              std::vector<Features::Preview::State::ViewportVertex>& vertices)
    -> void {
  vertices.clear();

  if (!state.preview.viewport.visible) {
    return;
  }

  // 获取预览窗口客户区大小
  RECT clientRect;
  GetClientRect(state.preview.hwnd, &clientRect);
  float previewWidth = static_cast<float>(clientRect.right - clientRect.left);
  float previewHeight =
      static_cast<float>(clientRect.bottom - clientRect.top - state.preview.dpi_sizes.title_height);

  if (previewWidth <= 0 || previewHeight <= 0) {
    return;
  }

  // 计算可见区域在预览窗口中的相对位置
  RECT visibleArea = calculate_visible_game_area(state);
  RECT gameRect = state.preview.game_window_rect;

  float gameWidth = static_cast<float>(gameRect.right - gameRect.left);
  float gameHeight = static_cast<float>(gameRect.bottom - gameRect.top);

  if (gameWidth <= 0 || gameHeight <= 0) {
    return;
  }

  // 计算视口框在预览窗口中的归一化坐标 (0-1)
  float viewportLeft = static_cast<float>(visibleArea.left - gameRect.left) / gameWidth;
  float viewportTop = static_cast<float>(visibleArea.top - gameRect.top) / gameHeight;
  float viewportRight = static_cast<float>(visibleArea.right - gameRect.left) / gameWidth;
  float viewportBottom = static_cast<float>(visibleArea.bottom - gameRect.top) / gameHeight;

  // 限制在0-1范围内
  viewportLeft = std::clamp(viewportLeft, 0.0f, 1.0f);
  viewportTop = std::clamp(viewportTop, 0.0f, 1.0f);
  viewportRight = std::clamp(viewportRight, 0.0f, 1.0f);
  viewportBottom = std::clamp(viewportBottom, 0.0f, 1.0f);

  // 视口框颜色 RGBA(255, 160, 80, 0.8)
  Features::Preview::State::ViewportVertex::Color frameColor = {255.0f / 255.0f, 160.0f / 255.0f,
                                                                80.0f / 255.0f, 0.8f};

  // 创建矩形框线条顶点（4条边，8个顶点）
  vertices.reserve(8);

  // 上边
  vertices.push_back({{viewportLeft, viewportTop}, frameColor});
  vertices.push_back({{viewportRight, viewportTop}, frameColor});

  // 右边
  vertices.push_back({{viewportRight, viewportTop}, frameColor});
  vertices.push_back({{viewportRight, viewportBottom}, frameColor});

  // 下边
  vertices.push_back({{viewportRight, viewportBottom}, frameColor});
  vertices.push_back({{viewportLeft, viewportBottom}, frameColor});

  // 左边
  vertices.push_back({{viewportLeft, viewportBottom}, frameColor});
  vertices.push_back({{viewportLeft, viewportTop}, frameColor});
}

auto calculate_viewport_position(const Core::State::AppState& state) -> RECT {
  RECT result = {0, 0, 0, 0};

  if (!state.preview.hwnd || !state.preview.target_window) {
    return result;
  }

  // 获取预览窗口客户区
  RECT clientRect;
  GetClientRect(state.preview.hwnd, &clientRect);

  // 计算预览区域（除去标题栏）
  int previewTop = state.preview.dpi_sizes.title_height;
  int previewWidth = clientRect.right - clientRect.left;
  int previewHeight = clientRect.bottom - clientRect.top - previewTop;

  if (previewWidth <= 0 || previewHeight <= 0) {
    return result;
  }

  // 获取可见区域相对游戏窗口的比例
  RECT visibleArea = calculate_visible_game_area(state);
  RECT gameRect = state.preview.game_window_rect;

  float gameWidth = static_cast<float>(gameRect.right - gameRect.left);
  float gameHeight = static_cast<float>(gameRect.bottom - gameRect.top);

  if (gameWidth <= 0 || gameHeight <= 0) {
    return result;
  }

  // 计算视口框在预览窗口中的像素位置
  float relativeLeft = static_cast<float>(visibleArea.left - gameRect.left) / gameWidth;
  float relativeTop = static_cast<float>(visibleArea.top - gameRect.top) / gameHeight;
  float relativeRight = static_cast<float>(visibleArea.right - gameRect.left) / gameWidth;
  float relativeBottom = static_cast<float>(visibleArea.bottom - gameRect.top) / gameHeight;

  result.left = static_cast<LONG>(relativeLeft * previewWidth);
  result.top = static_cast<LONG>(relativeTop * previewHeight) + previewTop;
  result.right = static_cast<LONG>(relativeRight * previewWidth);
  result.bottom = static_cast<LONG>(relativeBottom * previewHeight) + previewTop;

  return result;
}

auto get_game_window_screen_rect(const Core::State::AppState& state) -> RECT {
  RECT rect = {0, 0, 0, 0};

  if (state.preview.target_window && IsWindow(state.preview.target_window)) {
    GetWindowRect(state.preview.target_window, &rect);
  }

  return rect;
}

auto calculate_visible_game_area(const Core::State::AppState& state) -> RECT {
  // 获取屏幕可见区域（即屏幕边界）
  RECT screenRect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
  RECT gameRect = get_game_window_screen_rect(state);

  // 计算游戏窗口与屏幕的交集（可见部分）
  RECT visibleRect;
  if (!IntersectRect(&visibleRect, &gameRect, &screenRect)) {
    // 如果没有交集，返回空矩形
    visibleRect = {0, 0, 0, 0};
  }

  return visibleRect;
}

}  // namespace Features::Preview::Viewport