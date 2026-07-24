#pragma once

#include <asio.hpp>
#include <rfl/json.hpp>

#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"
#include "vendor/build_config.hpp"

namespace Core::RPC {

// 异步处理器签名
template <typename Request, typename Response>
using AsyncHandler =
    std::move_only_function<RpcAwaitable<Response>(Core::State::AppState&, const Request&) const>;

// 创建标准错误响应
auto create_error_response(rfl::Generic request_id, ErrorCode error_code,
                           const std::string& message) -> std::string;

// 处理JSON-RPC请求
auto process_request(Core::State::AppState& app_state, const std::string& request_json)
    -> RpcJsonAwaitable;

// 注册 RPC 方法：擦除业务处理器类型并生成统一的 JSON-RPC 协程入口
template <typename Request, typename Response>
inline auto register_method(Core::State::AppState& app_state,
                            std::unordered_map<std::string, MethodInfo>& registry,
                            const std::string& method_name, AsyncHandler<Request, Response> handler,
                            const std::string& description = "") -> void {
  // 注册表独占业务处理器，包装层只保留可重复 const 调用能力
  auto wrapped_handler = [handler = std::move(handler), &app_state](
                             rfl::Generic params_generic, rfl::Generic id) -> RpcJsonAwaitable {
    // 把通用 JSON 参数转换为当前方法的强类型请求
    auto request_result =
        rfl::from_generic<Request, rfl::SnakeCaseToCamelCase, rfl::DefaultIfMissing>(
            params_generic);
    if (!request_result) {
      co_return create_error_response(id, ErrorCode::InvalidParams,
                                      "Invalid parameters: " + request_result.error().what());
    }

    // 调用业务协程并保留统一的 AppState 注入方式
    auto result = co_await handler(app_state, request_result.value());

    // 将业务结果转换为 JSON-RPC 成功或错误响应
    if (result) {
      JsonRpcSuccessResponse success_response;
      success_response.id = id;
      success_response.result = rfl::to_generic<rfl::SnakeCaseToCamelCase>(result.value());
      co_return rfl::json::write<rfl::SnakeCaseToCamelCase>(success_response);
    } else {
      const auto& error = result.error();
      Logger().error("Error response: {}", error.message);
      co_return create_error_response(id, static_cast<ErrorCode>(error.code), error.message);
    }
  };

  std::string params_schema;
  if constexpr (Vendor::BuildConfig::rpc_json_schema_enabled()) {
    params_schema = rfl::json::to_schema<Request, rfl::SnakeCaseToCamelCase>();
  }

  // 注册表取得包装处理器的唯一所有权
  registry[method_name] = MethodInfo{.name = method_name,
                                     .description = description,
                                     .params_schema = std::move(params_schema),
                                     .handler = std::move(wrapped_handler)};
}

}  // namespace Core::RPC
