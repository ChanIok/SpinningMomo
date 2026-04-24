module;

export module Features.WindowControl;

import std;
import Core.State;
import Vendor.Windows;

namespace Features::WindowControl {

// 窗口信息结构体
export struct WindowInfo {
  Vendor::Windows::HWND handle = nullptr;  // 添加默认值
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
  bool activate_window = true;
  std::optional<Vendor::Windows::HWND> letterbox_window = std::nullopt;
};

// 分辨率预设的几何信息。
// base_width/base_height 来自菜单预设（如 1920x1080）；0x0 表示 Default，按屏幕尺寸计算。
export struct ResolutionPresetInput {
  int base_width = 0;
  int base_height = 0;
};

// 窗口尺寸计算选项，仅影响比例/分辨率菜单产生的目标窗口尺寸。
// 重置窗口命令直接应用屏幕尺寸或固定宽高，不走这些规则。
export struct ResolutionCalculationOptions {
  bool align_to_8 = false;
  bool use_short_edge = false;
};

// 查找目标窗口
export auto find_target_window(const std::wstring& configured_title)
    -> std::expected<Vendor::Windows::HWND, std::string>;

// 获取所有可见窗口的列表
export auto get_visible_windows() -> std::vector<WindowInfo>;

// 切换窗口边框
export auto toggle_window_border(Vendor::Windows::HWND hwnd) -> std::expected<bool, std::string>;

// 分辨率计算函数
export auto calculate_resolution_by_area(double ratio, std::uint64_t total_area) -> Resolution;
export auto calculate_resolution_by_screen(double ratio) -> Resolution;
export auto calculate_resolution_from_preset(double ratio,
                                             const ResolutionPresetInput& resolution_preset,
                                             const ResolutionCalculationOptions& options = {})
    -> Resolution;

// 窗口变换操作
export auto apply_window_transform(Core::State::AppState& state,
                                   Vendor::Windows::HWND target_window,
                                   const Resolution& resolution,
                                   const TransformOptions& options = {})
    -> std::expected<void, std::string>;

export auto reset_window_to_screen(Core::State::AppState& state,
                                   Vendor::Windows::HWND target_window,
                                   const TransformOptions& options = {})
    -> std::expected<void, std::string>;

// 调整窗口大小并居中 (保持向后兼容)
export auto resize_and_center_window(Core::State::AppState& state, Vendor::Windows::HWND hwnd,
                                     int width, int height, bool activate)
    -> std::expected<void, std::string>;

export auto start_center_lock_monitor(Core::State::AppState& state)
    -> std::expected<void, std::string>;

export auto stop_center_lock_monitor(Core::State::AppState& state) -> void;

}  // namespace Features::WindowControl
