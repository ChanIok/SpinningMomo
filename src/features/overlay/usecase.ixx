module;

export module Features.Overlay.UseCase;

import Core.State;
import UI.AppWindow.Events;

namespace Features::Overlay::UseCase {

// 处理叠加层功能切换
export auto handle_overlay_toggle(Core::State::AppState& state,
                                 const UI::AppWindow::Events::OverlayToggleEvent& event) -> void;

}  // namespace Features::Overlay::UseCase