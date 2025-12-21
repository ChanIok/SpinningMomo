module;

export module Features.WindowControl.UseCase;

import Core.State;
import UI.FloatingWindow.Events;

namespace Features::WindowControl::UseCase {

// 处理比例改变事件
export auto handle_ratio_changed(Core::State::AppState& state,
                                 const UI::FloatingWindow::Events::RatioChangeEvent& event) -> void;

// 处理分辨率改变事件
export auto handle_resolution_changed(
    Core::State::AppState& state, const UI::FloatingWindow::Events::ResolutionChangeEvent& event)
    -> void;

// 处理窗口选择事件
export auto handle_window_selected(Core::State::AppState& state,
                                   const UI::FloatingWindow::Events::WindowSelectionEvent& event)
    -> void;

// 重置窗口变换（直接调用版本）
export auto reset_window_transform(Core::State::AppState& state) -> void;

}  // namespace Features::WindowControl::UseCase
