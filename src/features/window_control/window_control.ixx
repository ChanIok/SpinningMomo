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

// 查找目标窗口
export auto find_target_window(const std::wstring& configured_title)
    -> std::expected<Vendor::Windows::HWND, std::string>;

// 调整窗口大小并居中
export auto resize_and_center_window(Vendor::Windows::HWND hwnd, int width, int height,
                                     bool taskbar_lower, bool activate)
    -> std::expected<void, std::string>;

// 切换窗口边框
export auto toggle_window_border(Vendor::Windows::HWND hwnd) -> std::expected<bool, std::string>;

// 获取所有可见窗口的列表
export auto get_visible_windows() -> std::vector<WindowInfo>;

}  // namespace Features::WindowControl