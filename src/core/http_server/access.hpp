#pragma once

#include "vendor/std.hpp"

#include "core/http_server/network_addresses.hpp"
#include "core/rpc/types.hpp"

namespace core {
struct AppState;
}

namespace core::http_server::access {

// 汇总 LAN 开关、运行状态、令牌和当前可用网卡，供本机设置页生成访问链接。
struct RuntimeInfo {
  bool configured_enabled = false;        // 设置中是否启用 LAN
  bool runtime_enabled = false;           // 当前进程是否已按 LAN 范围监听
  bool restart_required = false;          // 配置与运行状态是否不一致
  int port = 0;                           // 当前实际监听端口
  std::string token;                      // 当前运行时令牌
  std::vector<NetworkAddress> addresses;  // 实时枚举的可用地址
};

// 加载或生成令牌，并把令牌发布到 HTTP 服务运行时状态。
auto initialize(core::AppState& state) -> std::expected<void, std::string>;
// 生成新令牌、持久化并立即撤销已有远端会话。
auto reset_token(core::AppState& state) -> std::expected<std::string, std::string>;
// 读取当前 LAN 配置、实时网卡地址和令牌状态。
auto get_runtime_info(const core::AppState& state) -> std::expected<RuntimeInfo, std::string>;

// 判断文本地址是否属于本机回环接口。
auto is_loopback_address(std::string_view address) -> bool;
// 当前 HTTP 服务已经监听局域网且配置仍允许远程访问。
auto is_remote_access_enabled(const core::AppState& state) -> bool;
// 从 Cookie 头中提取指定名称的值。
auto extract_cookie(std::string_view cookie_header, std::string_view name)
    -> std::optional<std::string>;
// 在令牌比较时使用常数时间比较，避免泄露匹配长度信息。
auto is_token_valid(const core::AppState& state, std::string_view token) -> bool;
// 根据来源地址、服务状态和 Cookie 判定 HTTP 请求的访问等级。
auto resolve_http_access(const core::AppState& state, std::string_view remote_address,
                         std::string_view cookie_header) -> std::optional<core::rpc::AccessLevel>;
// 构造只允许脚本携带的 LAN 会话 Cookie。
auto make_cookie(std::string_view token) -> std::string;

}  // namespace core::http_server::access
