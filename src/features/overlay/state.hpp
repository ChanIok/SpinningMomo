#pragma once

#include "vendor/std.hpp"

#include "features/overlay/types.hpp"

namespace features::overlay {

// 叠加层完整状态
struct OverlayState {
  WindowState window;
  RenderingState rendering;
  CaptureState capture_state;
  InteractionState interaction;
  ThreadState threads;
  bool enable_hdr = false;

  std::condition_variable frame_available;

  // 状态标志
  bool enabled = false;                                // 用户是否启用叠加层模式
  std::atomic<bool> running = false;                   // 叠加层是否实际在运行
  std::atomic<bool> is_transforming = false;           // 窗口变换流程进行中
  std::atomic<bool> freeze_rendering = false;          // 冻结渲染（保持最后一帧）
  std::atomic<bool> freeze_after_first_frame = false;  // 首帧渲染后自动冻结
};

}  // namespace features::overlay
