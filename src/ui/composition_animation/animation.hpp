#pragma once

#include "vendor/std.hpp"
#include "vendor/wil.hpp"
#include "vendor/windows.hpp"
#include "vendor/windows/dcomp.hpp"

namespace ui::composition_animation {

// 与合成器共享 QPC 时间和三次缓出曲线；CPU 采样用于输入检测和动画重定向。
struct Transition {
  float from = 0.0f;
  float to = 0.0f;
  LARGE_INTEGER begin{};
  double seconds = 0.0;
};

// 获取当前 QPC 时间戳
auto now() -> LARGE_INTEGER;

// 在 CPU 端采样过渡动画在指定 QPC 时刻的插值
auto sample(const Transition& transition, LARGE_INTEGER time) -> float;

// 基于三次缓出曲线创建 DComp 硬件动画对象
auto create(IDCompositionDevice* device, const Transition& transition)
    -> std::expected<wil::com_ptr<IDCompositionAnimation>, HRESULT>;

}  // namespace ui::composition_animation
