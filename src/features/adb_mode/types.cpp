#include "features/adb_mode/types.hpp"

#include "vendor/std.hpp"

namespace features::adb_mode {

// 将连接状态映射成前端可稳定识别的英文标识。
auto connection_state_to_string(ConnectionState state) -> std::string {
  switch (state) {
    case ConnectionState::Disconnected:
      return "disconnected";
    case ConnectionState::Connecting:
      return "connecting";
    case ConnectionState::Connected:
      return "connected";
    case ConnectionState::Restoring:
      return "restoring";
    case ConnectionState::Error:
      return "error";
  }

  return "unknown";
}

}  // namespace features::adb_mode
