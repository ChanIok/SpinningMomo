#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "features/preview/types.hpp"

namespace features::preview {

// 预览窗口完整状态
struct PreviewState {
  // 窗口句柄
  HWND hwnd = nullptr;
  HWND target_window = nullptr;

  // 窗口状态
  bool is_first_show = true;

  // 尺寸相关
  features::preview::WindowSizeState size;
  features::preview::DpiDependentSizes dpi_sizes;

  // 交互状态
  features::preview::InteractionState interaction;
  features::preview::ViewportState viewport;

  // 渲染状态
  std::atomic<bool> running = false;
  std::atomic<bool> create_new_srv = true;
  bool enable_hdr = false;
  features::preview::RenderingResources rendering_resources;

  // 捕获状态
  features::preview::CaptureState capture_state;

  // 游戏窗口缓存信息
  RECT game_window_rect{};

  // 当前预览会话的基准显示器。启动预览时由悬浮窗所在工作显示器确定。
  RECT screen_rect{};
  bool has_screen_rect = false;
};

}  // namespace features::preview
