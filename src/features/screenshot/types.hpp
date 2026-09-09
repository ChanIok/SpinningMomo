#pragma once

#include "vendor/std.hpp"

namespace features::screenshot {

// 单次截图的主图与可选 JXR 输出结果。
// 主图失败时仍会独立尝试保存 JXR，避免一份编码失败掩盖另一份可用输出。
struct ScreenshotSaveResult {
  bool success = false;
  std::wstring path;
  std::string error;
  bool jxr_requested = false;
  bool jxr_success = false;
  std::wstring jxr_path;
  std::string jxr_error;
};

}  // namespace features::screenshot
