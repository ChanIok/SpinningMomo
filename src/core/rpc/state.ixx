module;

export module Core.RPC.State;

import std;
import Core.RPC.Types;

namespace Core::RPC::State {

export struct RpcState {
  std::unordered_map<std::string, MethodInfo> registry;
};

}  // namespace Core::RPC::State
