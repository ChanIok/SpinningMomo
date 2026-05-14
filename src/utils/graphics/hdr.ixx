module;

#include <windows.h>

export module Utils.Graphics.Hdr;

import std;

// 查询窗口所在 DXGI 输出的 HDR 描述：是否处于 HDR10/G2084 色彩空间、面板上报的峰值亮度等。

namespace Utils::Graphics::Hdr {

export struct HdrMonitorInfo {
  // 当前输出色彩空间是否为典型 HDR10（PQ + BT.2020 容器）路径上的 RGB_FULL_G2084_NONE_P2020。
  bool hdr_active = false;
  // MaxLuminance（nit），已裁剪到合理范围；写入 Ultra HDR 元数据或作参考。
  float max_luminance_nits = 1000.0f;
};

export auto query_monitor_hdr_info(HWND target_window)
    -> std::expected<HdrMonitorInfo, std::string>;

}  // namespace Utils::Graphics::Hdr
