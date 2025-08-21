module;

export module Features.Letterbox.UseCase;

import Core.State;
import UI.AppWindow.Events;

export namespace Features::Letterbox::UseCase {

// 处理letterbox功能切换
export auto handle_letterbox_toggle(Core::State::AppState& state,
                                    const UI::AppWindow::Events::LetterboxToggleEvent& event) -> void;

}  // namespace Features::Letterbox::UseCase