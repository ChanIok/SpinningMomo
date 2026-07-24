#pragma once

#include "core/rpc/types.hpp"

namespace Core::RPC::State {

struct RpcState {
  std::unordered_map<std::string, MethodInfo> registry;
};

}  // namespace Core::RPC::State
