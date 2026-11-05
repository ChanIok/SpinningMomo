#pragma once

#include "vendor/std.hpp"

#include "vendor/windows.hpp"

#include "core/state/app_state.hpp"
#include "features/recording/types.hpp"

namespace features::recording::video {

// 计算捕获范围与裁剪计划：根据窗口客户区或全窗口确定偶数对齐的输出尺寸
auto resolve_capture_plan(HWND window, bool client_area, int width, int height)
    -> std::expected<CapturePlan, std::string>;

// 视频工作线程主循环：创建 D3D11 设备 → 协商硬件/软件 MFT → 启动 WGC 捕获 → 帧转换与编码输出 →
// 排空收尾
auto run(core::AppState& app_state) -> std::expected<void, std::string>;

}  // namespace features::recording::video
