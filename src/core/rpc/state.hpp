#pragma once

#include "vendor/std.hpp"

#include "core/rpc/types.hpp"

namespace core::rpc {

struct RpcState {
  std::unordered_map<std::string, MethodInfo> registry;
};

}  // namespace core::rpc
