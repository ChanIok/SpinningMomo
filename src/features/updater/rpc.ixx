module;

export module Features.Updater.Rpc;

import std;
import Core.State;

namespace Features::Updater::Rpc {

export auto register_handlers(Core::State::AppState& app_state) -> void;

}  // namespace Features::Updater::Rpc