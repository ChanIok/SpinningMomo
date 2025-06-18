module;

export module Features.WindowControl;

import std;
import Vendor.Windows;

namespace Features::WindowControl {

// 窗口信息结构体
export struct WindowInfo {
  Vendor::Windows::HWND handle;
  std::wstring title;

  auto operator==(const WindowInfo& other) const noexcept -> bool {
    return handle == other.handle && title == other.title;
  }
};

// 分辨率结构体
export struct Resolution {
  int width;
  int height;

  auto operator==(const Resolution& other) const noexcept -> bool {
    return width == other.width && height == other.height;
  }
};

// 窗口变换选项
export struct TransformOptions {
  bool taskbar_lower = false;
  bool activate_window = true;
  std::optional<Vendor::Windows::HWND> letterbox_window = std::nullopt;
};

// 查找目标窗口
export auto find_target_window(const std::wstring& configured_title)
    -> std::expected<Vendor::Windows::HWND, std::string>;

// 获取所有可见窗口的列表
export auto get_visible_windows() -> std::vector<WindowInfo>;

// 切换窗口边框
export auto toggle_window_border(Vendor::Windows::HWND hwnd) -> std::expected<bool, std::string>;

// 分辨率计算函数
export auto calculate_resolution(double ratio, std::uint64_t total_pixels) -> Resolution;
export auto calculate_resolution_by_screen(double ratio) -> Resolution;

// 窗口变换操作
export auto apply_window_transform(Vendor::Windows::HWND target_window, 
                                   const Resolution& resolution,
                                   const TransformOptions& options = {})
    -> std::expected<void, std::string>;

export auto reset_window_to_screen(Vendor::Windows::HWND target_window,
                                   const TransformOptions& options = {})
    -> std::expected<void, std::string>;

// 调整窗口大小并居中 (保持向后兼容)
export auto resize_and_center_window(Vendor::Windows::HWND hwnd, int width, int height,
                                     bool taskbar_lower, bool activate)
    -> std::expected<void, std::string>;

}  // namespace Features::WindowControl