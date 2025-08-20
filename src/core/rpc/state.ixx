module;

export module Core.RPC.State;

import std;
import Core.RPC.Types;

export namespace Core::RPC::State {

struct RpcState {
  std::unordered_map<std::string, MethodInfo> registry;
};

}  // namespace Core::RPC::State
