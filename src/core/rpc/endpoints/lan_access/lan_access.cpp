#include "core/rpc/endpoints/lan_access/lan_access.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/http_server/access.hpp"
#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"

namespace core::rpc::endpoints::lan_access {

// 本地设置页调用的空参数结构，保留后续扩展空间。
struct EmptyParams {};

// 将内部网卡信息投影为稳定的 RPC 返回结构，并附加访问链接。
struct NetworkAddressResult {
  std::string adapter_id;            // 稳定适配器标识
  std::string adapter_name;          // Windows 友好名称
  std::string ip;                    // 可访问的 IPv4 地址
  std::uint32_t metric = 0;          // 接口路由 metric
  bool has_default_gateway = false;  // 是否拥有默认网关
  bool is_virtual = false;           // 是否由 Windows 接口类型明确标记为虚拟接口
  bool is_private = false;           // 是否属于 RFC1918 局域网私有地址段
  bool is_preferred = false;         // 是否命中用户首选适配器
  std::string url;                   // 当前令牌生成的访问链接
};

// 汇总开关、运行状态、首选链接和所有可用网卡地址。
struct AccessInfoResult {
  bool configured_enabled = false;              // 设置中是否启用 LAN
  bool runtime_enabled = false;                 // 当前进程是否已按 LAN 范围监听
  bool restart_required = false;                // 配置和运行状态是否不一致
  int port = 0;                                 // 当前实际监听端口
  std::string preferred_url;                    // 排序后的首选访问链接
  std::vector<NetworkAddressResult> addresses;  // 所有可用地址
};

// 将 HTTP 访问状态转换成设置页使用的 DTO。
auto make_result(const core::AppState& app_state) -> std::expected<AccessInfoResult, std::string> {
  auto runtime_result = core::http_server::access::get_runtime_info(app_state);
  if (!runtime_result) {
    return std::unexpected(runtime_result.error());
  }

  AccessInfoResult result{
      .configured_enabled = runtime_result->configured_enabled,
      .runtime_enabled = runtime_result->runtime_enabled,
      .restart_required = runtime_result->restart_required,
      .port = runtime_result->port,
  };

  // 只有配置已启用、服务已按配置运行且令牌有效时才生成可分享链接。
  const bool can_share_url = runtime_result->configured_enabled &&
                             runtime_result->runtime_enabled && runtime_result->port > 0 &&
                             !runtime_result->token.empty();

  // 为每个地址生成独立链接，首个地址由后端排序结果决定为首选链接。
  for (const auto& address : runtime_result->addresses) {
    const auto url = can_share_url ? std::format("http://{}:{}/t/{}", address.ip,
                                                 runtime_result->port, runtime_result->token)
                                   : std::string{};
    result.addresses.push_back(NetworkAddressResult{
        .adapter_id = address.adapter_id,
        .adapter_name = address.adapter_name,
        .ip = address.ip,
        .metric = address.metric,
        .has_default_gateway = address.has_default_gateway,
        .is_virtual = address.is_virtual,
        .is_private = address.is_private,
        .is_preferred = address.is_preferred,
        .url = url,
    });
  }

  if (can_share_url && !result.addresses.empty()) {
    result.preferred_url = result.addresses.front().url;
  }
  return result;
}

// 返回当前 LAN 访问配置和网卡链接。
auto handle_get_info(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> asio::awaitable<core::rpc::RpcResult<AccessInfoResult>> {
  auto result = make_result(app_state);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = result.error(),
    });
  }
  co_return std::move(*result);
}

// 轮换令牌后重新生成完整的设置页状态。
auto handle_reset_token(core::AppState& app_state, [[maybe_unused]] const EmptyParams& params)
    -> asio::awaitable<core::rpc::RpcResult<AccessInfoResult>> {
  auto reset_result = core::http_server::access::reset_token(app_state);
  if (!reset_result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = reset_result.error(),
    });
  }

  auto result = make_result(app_state);
  if (!result) {
    co_return std::unexpected(core::rpc::RpcError{
        .code = static_cast<int>(core::rpc::ErrorCode::ServerError),
        .message = result.error(),
    });
  }
  co_return std::move(*result);
}

// 注册仅允许本机调用的 LAN 配置接口。
auto register_all(core::AppState& app_state) -> void {
  core::rpc::register_method<EmptyParams, AccessInfoResult>(
      app_state, app_state.rpc->registry, "lanAccess.getInfo", handle_get_info,
      "Get local LAN access status and URLs", core::rpc::AccessLevel::local);

  core::rpc::register_method<EmptyParams, AccessInfoResult>(
      app_state, app_state.rpc->registry, "lanAccess.resetToken", handle_reset_token,
      "Regenerate the local LAN access token", core::rpc::AccessLevel::local);
}

}  // namespace core::rpc::endpoints::lan_access
