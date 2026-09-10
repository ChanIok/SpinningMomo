#include "ui/composition_animation/animation.hpp"

#include "vendor/std.hpp"
#include "vendor/windows/dcompanimation.hpp"

namespace ui::composition_animation {

// 获取当前 QPC 时间戳，作为合成动画与采样的时间基准
auto now() -> LARGE_INTEGER {
  LARGE_INTEGER time{};
  QueryPerformanceCounter(&time);
  return time;
}

// 在 CPU 端采样过渡动画在指定时间的插值：无有效时长直接返回终点 → 算归一化时间 t → 三次缓出求值
auto sample(const Transition& transition, LARGE_INTEGER time) -> float {
  // 无有效时长或已到期时直接返回目标值
  if (transition.seconds <= 0.0) {
    return transition.to;
  }
  LARGE_INTEGER frequency{};
  QueryPerformanceFrequency(&frequency);
  // 基于 QPC 频率计算自起始时间以来的经过秒数
  const double elapsed = static_cast<double>(time.QuadPart - transition.begin.QuadPart) /
                         static_cast<double>(frequency.QuadPart);
  // 限制进度在 [0, 1] 范围
  const float t = static_cast<float>(std::clamp(elapsed / transition.seconds, 0.0, 1.0));
  // 计算三次缓出曲线 (ease-out cubic: 1 - (1-t)^3)
  const float remaining = 1.0f - t;
  return transition.from +
         (transition.to - transition.from) * (1.0f - remaining * remaining * remaining);
}

// 基于三次缓出曲线创建 DComp 硬件动画：创建动画对象 → 添加缓出曲线 → 设置终点 → 绑定 QPC 起始时间
auto create(IDCompositionDevice* device, const Transition& transition)
    -> std::expected<wil::com_ptr<IDCompositionAnimation>, HRESULT> {
  // 校验入参有效性
  if (!device || transition.seconds <= 0.0) {
    return std::unexpected(E_INVALIDARG);
  }
  wil::com_ptr<IDCompositionAnimation> result;
  HRESULT hr = device->CreateAnimation(result.put());
  const float duration = static_cast<float>(transition.seconds);
  const float delta = transition.to - transition.from;
  // 添加三次多项式曲线段，导数参数对应 ease-out cubic
  if (SUCCEEDED(hr)) {
    hr = result->AddCubic(0.0, transition.from, 3.0f * delta / duration,
                          -3.0f * delta / (duration * duration),
                          delta / (duration * duration * duration));
  }
  // 达到指定时长后锁定在终点值
  if (SUCCEEDED(hr)) {
    hr = result->End(transition.seconds, transition.to);
  }
  // 绑定绝对起始 QPC 时间，确保与系统合成器时间轴严格同步
  if (SUCCEEDED(hr) && transition.begin.QuadPart != 0) {
    hr = result->SetAbsoluteBeginTime(transition.begin);
  }
  if (FAILED(hr)) {
    return std::unexpected(hr);
  }
  return std::move(result);
}

}  // namespace ui::composition_animation
