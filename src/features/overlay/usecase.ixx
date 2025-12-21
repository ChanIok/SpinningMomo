module;

export module Features.Overlay.UseCase;

import Core.State;
import UI.FloatingWindow.Events;

namespace Features::Overlay::UseCase {

// 切换叠加层功能
export auto toggle_overlay(Core::State::AppState& state) -> void;

}  // namespace Features::Overlay::UseCase
