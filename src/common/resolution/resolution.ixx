module;

export module Common.Resolution;

import std;

// 分辨率工具命名空间
namespace Common::Resolution {

// 分辨率结构体
struct Resolution {
  int width;
  int height;
  std::uint64_t totalPixels;

  Resolution(int w = 0, int h = 0)
      : width(w), height(h), totalPixels(static_cast<std::uint64_t>(w) * h) {}

  // 便于比较
  auto operator==(const Resolution& other) const noexcept -> bool {
    return width == other.width && height == other.height;
  }

  auto operator!=(const Resolution& other) const noexcept -> bool { return !(*this == other); }

  // 获取宽高比
  auto aspectRatio() const noexcept -> double {
    return height > 0 ? static_cast<double>(width) / height : 0.0;
  }
};

// 根据总像素数和宽高比计算分辨率
// ratio: 宽高比，例如 16:9 = 16.0/9.0 ≈ 1.778
export auto CalculateResolution(std::uint64_t totalPixels, double ratio)
    -> std::expected<Resolution, std::string>;

// 根据屏幕分辨率和目标宽高比计算适合的分辨率
// targetRatio: 目标宽高比
export auto CalculateResolutionByScreen(double targetRatio)
    -> std::expected<Resolution, std::string>;

}  // namespace Common::Resolution