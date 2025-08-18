module;

export module Features.Settings.Rpc;

import std;
import Core.State;

namespace Features::Settings::Rpc {

export auto register_handlers(Core::State::AppState& app_state) -> void;

}  // namespace Features::Settings::Rpc