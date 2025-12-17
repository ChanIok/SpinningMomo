module;

export module Features.Settings.Compute;

import Core.State;

namespace Features::Settings::Compute {

// 更新状态的计算部分
// 触发计算状态更新 (Reactivity Trigger)
export auto trigger_compute(Core::State::AppState& app_state) -> bool;

}  // namespace Features::Settings::Compute