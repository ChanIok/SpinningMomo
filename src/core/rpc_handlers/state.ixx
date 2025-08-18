module;

export module Core.RpcHandlers.State;

import std;
import Core.RpcHandlers.Types;

export namespace Core::RpcHandlers::State {

struct RpcHandlerState {
  std::unordered_map<std::string, MethodInfo> registry;
};

}  // namespace Core::RpcHandlers::State