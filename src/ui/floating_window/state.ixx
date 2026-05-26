module;

export module UI.FloatingWindow.State;

import std;
import UI.FloatingWindow.Types;

namespace UI::FloatingWindow::State {

// 主窗口聚合状态
export struct FloatingWindowState {
  FloatingWindow::WindowInfo window;
  FloatingWindow::InteractionState ui;
  FloatingWindow::DataState data;
  FloatingWindow::LayoutConfig layout;
  FloatingWindow::RenderContext d2d_context;  // 私有的D2D渲染上下文
};

}  // namespace UI::FloatingWindow::State
