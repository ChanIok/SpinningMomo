module;

export module Features.Preview.UseCase;

import Core.State;
import UI.AppWindow.Events;

namespace Features::Preview::UseCase {

// 切换预览功能
export auto toggle_preview(Core::State::AppState& state) -> void;

}  // namespace Features::Preview::UseCase
