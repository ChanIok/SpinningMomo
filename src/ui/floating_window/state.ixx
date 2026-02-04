module;

export module UI.FloatingWindow.State;

import std;
import Features.Settings.Menu;
import Core.Commands;
import Core.Commands.State;
import UI.FloatingWindow.Types;
import Core.State;

export namespace UI::FloatingWindow::State {

// 主窗口聚合状态
struct FloatingWindowState {
  FloatingWindow::WindowInfo window;
  FloatingWindow::InteractionState ui;
  FloatingWindow::DataState data;
  FloatingWindow::LayoutConfig layout;
  FloatingWindow::RenderContext d2d_context;  // 私有的D2D渲染上下文
};

// 辅助函数：判断菜单项是否选中
auto is_item_selected(const FloatingWindow::MenuItem& item, const Core::State::AppState& app_state)
    -> bool {
  switch (item.category) {
    case FloatingWindow::MenuItemCategory::AspectRatio:
      return item.index == static_cast<int>(app_state.floating_window->ui.current_ratio_index);
    case FloatingWindow::MenuItemCategory::Resolution:
      return item.index == static_cast<int>(app_state.floating_window->ui.current_resolution_index);
    case FloatingWindow::MenuItemCategory::Feature: {
      // 从命令注册表查询状态
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

}  // namespace UI::FloatingWindow::State
