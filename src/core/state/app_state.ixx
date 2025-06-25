module;

export module Core.State;

import std;
import Core.Events;
import Types.Config;
import Types.State;
import Types.UI;
import Types.Notification;
import Types.TrayIcon;
import Types.Preview;
import Types.Screenshot;
import Vendor.Windows;

namespace Core::State {

export using ::Types::State::DataState;
export using ::Types::State::ItemType;
export using ::Types::State::MenuItem;
export using ::Types::State::RenderState;
export using ::Types::State::UIState;
export using ::Types::State::WindowState;
export using ::Types::UI::D2DRenderState;

// 应用程序状态（所有状态的聚合）
export struct AppState {
  Types::Config::AppConfig config;
  WindowState window;
  UIState ui;
  DataState data;
  RenderState render;
  Types::UI::D2DRenderState d2d_render;  // 新增Direct2D渲染状态
  Core::Events::EventBus event_bus;
  Types::Notification::NotificationSystemState notifications;
  Types::TrayIcon::Data tray_icon;
  Types::Preview::PreviewState preview;
  Types::Screenshot::ScreenshotState screenshot;

  // 便捷访问方法
  auto is_window_valid() const -> bool { return window.hwnd != nullptr; }
  auto get_total_width() const -> int {
    return render.ratio_column_width + render.resolution_column_width +
           render.settings_column_width;
  }
  auto get_menu_item_count() const -> size_t { return data.menu_items.size(); }
};

// 根据DPI更新渲染状态
export auto update_render_dpi(AppState& state, Vendor::Windows::UINT new_dpi) -> void {
  state.window.dpi = new_dpi;
  state.render.update_dpi_scaling(new_dpi);
}

// 检查项目是否被选中
export auto is_item_selected(const MenuItem& item, const UIState& ui_state) -> bool {
  switch (item.type) {
    case ItemType::Ratio:
      return item.index == static_cast<int>(ui_state.current_ratio_index);
    case ItemType::Resolution:
      return item.index == static_cast<int>(ui_state.current_resolution_index);
    case ItemType::PreviewWindow:
      return ui_state.preview_enabled;
    case ItemType::OverlayWindow:
      return ui_state.overlay_enabled;
    case ItemType::LetterboxWindow:
      return ui_state.letterbox_enabled;
    default:
      return false;
  }
}

}  // namespace Core::State