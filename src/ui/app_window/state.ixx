module;

export module UI.AppWindow.State;

import std;
import Features.Settings.Menu;
import Core.Commands;
import Core.Commands.State;
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
      if (item.action_id == "preview.toggle") {
        return ui_state.preview_enabled;
      } else if (item.action_id == "overlay.toggle") {
        return ui_state.overlay_enabled;
      } else if (item.action_id == "letterbox.toggle") {
        return ui_state.letterbox_enabled;
      } else if (item.action_id == "recording.toggle") {
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
      // 从注册表查询命令状态
      if (app_state.commands) {
        if (auto command_opt =
                Core::Commands::get_command(app_state.commands->registry, item.action_id)) {
          if (command_opt->is_toggle && command_opt->get_state) {
            return command_opt->get_state();
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
