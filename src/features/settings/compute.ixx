module;

export module Features.Settings.Compute;

import Core.State;

namespace Features::Settings::Compute {

// 更新状态的计算部分
export auto update_computed_state(Core::State::AppState& app_state) -> bool;

}  // namespace Features::Settings::Compute