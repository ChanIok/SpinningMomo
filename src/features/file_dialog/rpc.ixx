module;

export module Features.FileDialog.Rpc;

import Core.State;

namespace Features::FileDialog::Rpc {

export auto register_handlers(Core::State::AppState& app_state) -> void;

}  // namespace Features::FileDialog::Rpc