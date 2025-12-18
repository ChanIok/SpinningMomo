module;

export module UI.AppWindow.State;

import std;
import Features.Settings.Menu;
import Features.Registry;
import UI.AppWindow.Types;
import Core.State;

export namespace UI::AppWindow::State {

// 主窗口聚合状态
struct AppWindowState {
  AppWindow::WindowInfo window;
  AppWindow::InteractionState ui;
  AppWindow::DataState data;
  AppWindow::LayoutConfig layout;
  AppWindow::RenderContext d2d_context;  // 私有的D2D渲染上下文
};

// 辅助函数
auto is_item_selected(const AppWindow::MenuItem& item, const AppWindow::InteractionState& ui_state)
    -> bool {
  switch (item.category) {
    case AppWindow::MenuItemCategory::AspectRatio:
      return item.index == static_cast<int>(ui_state.current_ratio_index);
    case AppWindow::MenuItemCategory::Resolution:
      return item.index == static_cast<int>(ui_state.current_resolution_index);
    case AppWindow::MenuItemCategory::Feature: {
      // 基于 action_id 判断功能项的选中状态
      if (item.action_id == "feature.toggle_preview") {
        return ui_state.preview_enabled;
      } else if (item.action_id == "feature.toggle_overlay") {
        return ui_state.overlay_enabled;
      } else if (item.action_id == "feature.toggle_letterbox") {
        return ui_state.letterbox_enabled;
      } else if (item.action_id == "feature.toggle_recording") {
        return ui_state.recording_enabled;
      }
      return false;
    }
    default:
      return false;
  }
}

}  // namespace UI::AppWindow::State

export namespace UI::AppWindow::State {

// 重载版本：支持 AppState 以便访问注册表
auto is_item_selected(const AppWindow::MenuItem& item, const Core::State::AppState& app_state)
    -> bool {
  switch (item.category) {
    case AppWindow::MenuItemCategory::AspectRatio:
      return item.index == static_cast<int>(app_state.app_window->ui.current_ratio_index);
    case AppWindow::MenuItemCategory::Resolution:
      return item.index == static_cast<int>(app_state.app_window->ui.current_resolution_index);
    case AppWindow::MenuItemCategory::Feature: {
      // 从注册表查询功能状态
      if (app_state.feature_registry) {
        if (auto feature_opt =
                Features::Registry::get_feature(*app_state.feature_registry, item.action_id)) {
          if (feature_opt->is_toggle && feature_opt->get_state) {
            return feature_opt->get_state();
          }
        }
      }
      return false;
    }
    default:
      return false;
  }
}

}  // namespace UI::AppWindow::State
