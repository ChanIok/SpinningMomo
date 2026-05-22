module;

export module Features.WindowControl.Types;

import std;
import Vendor.Windows;

namespace Features::WindowControl {

export struct WindowInfo {
  Vendor::Windows::HWND handle = nullptr;
  std::wstring title;

  auto operator==(const WindowInfo& other) const noexcept -> bool {
    return handle == other.handle && title == other.title;
  }
};

export struct Resolution {
  int width;
  int height;

  auto operator==(const Resolution& other) const noexcept -> bool {
    return width == other.width && height == other.height;
  }
};

export struct TransformOptions {
  bool activate_window = true;
  std::optional<Vendor::Windows::HWND> letterbox_window = std::nullopt;
};

export struct ResolutionPresetInput {
  int base_width = 0;
  int base_height = 0;
};

export struct ResolutionCalculationOptions {
  bool align_to_8 = false;
  bool use_short_edge = false;
  int screen_width = 0;
  int screen_height = 0;
};

}  // namespace Features::WindowControl
