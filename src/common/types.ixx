module;

export module Common.Types;

import std;

// 通用数据类型定义
export namespace Common::Types {

// 比例预设结构体
struct RatioPreset {
  std::wstring name;  // 比例名称
  double ratio;       // 宽高比值

  constexpr RatioPreset(const std::wstring& n, double r) noexcept : name(n), ratio(r) {}
};

// 分辨率预设结构体
struct ResolutionPreset {
  std::wstring name;          // 显示名称（如 "4K"）
  std::uint64_t totalPixels;  // 总像素数
  int baseWidth;              // 基准宽度
  int baseHeight;             // 基准高度

  constexpr ResolutionPreset(const std::wstring& n, int w, int h) noexcept
      : name(n), totalPixels(static_cast<std::uint64_t>(w) * h), baseWidth(w), baseHeight(h) {}
};

}  // namespace Common::Types