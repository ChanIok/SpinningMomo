module;

export module Core.RPC.Endpoints.Backup;

import Core.State;

namespace Core::RPC::Endpoints::Backup {

// 注册数据导出和完全替换恢复端点。
export auto register_all(Core::State::AppState& app_state) -> void;

}  // namespace Core::RPC::Endpoints::Backup
