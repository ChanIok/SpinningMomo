module;

module Common.Resolution;

import std;
import Vendor.Windows;

// 根据总像素数和宽高比计算分辨率
auto Common::Resolution::CalculateResolution(std::uint64_t totalPixels, double ratio)
    -> std::expected<Resolution, std::string> {
  try {
    if (totalPixels == 0) {
      return std::unexpected("Total pixels cannot be zero");
    }

    if (ratio <= 0.0) {
      return std::unexpected("Aspect ratio must be positive");
    }

    // ratio 是宽高比，例如 16:9 = 1.778
    // width * height = totalPixels
    // width / height = ratio
    // 所以 width = sqrt(totalPixels * ratio), height = width / ratio

    int width = static_cast<int>(std::sqrt(static_cast<double>(totalPixels) * ratio));
    int height = static_cast<int>(width / ratio);

    // 确保不是零
    if (width <= 0 || height <= 0) {
      return std::unexpected("Calculated dimensions are invalid");
    }

    // 微调以确保总像素数更准确
    std::uint64_t currentPixels = static_cast<std::uint64_t>(width) * height;
    if (currentPixels < totalPixels) {
      width++;
      // 重新计算高度以保持比例
      height = static_cast<int>(width / ratio);
      if (height <= 0) {
        height = 1;  // 最小值保护
      }
    }

    return Resolution(width, height);

  } catch (const std::exception& e) {
    return std::unexpected("Exception in CalculateResolution: " + std::string(e.what()));
  }
}

// 根据屏幕分辨率和目标宽高比计算适合的分辨率
auto Common::Resolution::CalculateResolutionByScreen(double targetRatio)
    -> std::expected<Resolution, std::string> {
  try {
    if (targetRatio <= 0.0) {
      return std::unexpected("Target aspect ratio must be positive");
    }

    int screenWidth = Vendor::Windows::GetScreenWidth();
    int screenHeight = Vendor::Windows::GetScreenHeight();

    if (screenWidth <= 0 || screenHeight <= 0) {
      return std::unexpected("Failed to get valid screen dimensions");
    }

    // 方案1：使用屏幕宽度计算高度
    int height1 = static_cast<int>(screenWidth / targetRatio);

    // 方案2：使用屏幕高度计算宽度
    int width2 = static_cast<int>(screenHeight * targetRatio);

    // 选择不超出屏幕的方案
    if (width2 <= screenWidth && width2 > 0) {
      // 如果基于高度计算的宽度不超出屏幕，使用方案2
      return Resolution(width2, screenHeight);
    } else if (height1 > 0) {
      // 否则使用方案1
      return Resolution(screenWidth, height1);
    } else {
      return std::unexpected("Cannot calculate valid resolution for given aspect ratio");
    }

  } catch (const std::exception& e) {
    return std::unexpected("Exception in CalculateResolutionByScreen: " + std::string(e.what()));
  }
}