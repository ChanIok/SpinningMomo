module;

export module Features.Letterbox.UseCase;

import Core.State;
import UI.AppWindow.Events;

namespace Features::Letterbox::UseCase {

// 切换黑边模式
export auto toggle_letterbox(Core::State::AppState& state) -> void;

}  // namespace Features::Letterbox::UseCase
