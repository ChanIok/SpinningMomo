module;

export module Features.WindowControl.UseCase;

import Core.State;
import UI.AppWindow.Events;

namespace Features::WindowControl::UseCase {

// 处理重置窗口事件
export auto handle_reset_event(Core::State::AppState& state,
                               const UI::AppWindow::Events::ResetEvent& event) -> void;

// 处理比例改变事件
export auto handle_ratio_changed(Core::State::AppState& state,
                                const UI::AppWindow::Events::RatioChangeEvent& event) -> void;

// 处理分辨率改变事件
export auto handle_resolution_changed(Core::State::AppState& state,
                                     const UI::AppWindow::Events::ResolutionChangeEvent& event) -> void;

// 处理窗口选择事件
export auto handle_window_selected(Core::State::AppState& state,
                                  const UI::AppWindow::Events::WindowSelectionEvent& event) -> void;

}  // namespace Features::WindowControl::UseCase