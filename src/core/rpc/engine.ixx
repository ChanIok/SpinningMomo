module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

export module Core.RPC.Engine;

import std;
import Core.State;
import Core.RPC.Types;
import Utils.Logger;

namespace Core::RPC {

// 异步处理器签名
template <typename Request, typename Response>
using AsyncHandler =
    std::function<asio::awaitable<RpcResult<Response>>(Core::State::AppState&, const Request&)>;

// 创建标准错误响应
export auto create_error_response(rfl::Generic request_id, ErrorCode error_code,
                                  const std::string& message) -> std::string;

// 处理JSON-RPC请求
export auto process_request(Core::State::AppState& app_state, const std::string& request_json)
    -> asio::awaitable<std::string>;

// 获取已注册方法列表
export auto get_method_list(const Core::State::AppState& app_state) -> std::vector<MethodListItem>;

// 检查方法是否存在
export auto method_exists(const Core::State::AppState& app_state, const std::string& method_name)
    -> bool;

// 注册RPC方法
export template <typename Request, typename Response>
auto register_method(Core::State::AppState& app_state,
                     std::unordered_map<std::string, MethodInfo>& registry,
                     const std::string& method_name, AsyncHandler<Request, Response> handler,
                     const std::string& description = "") -> void {
  // 创建类型擦除的处理器包装
  auto wrapped_handler = [handler, &app_state](rfl::Generic params_generic,
                                               rfl::Generic id) -> asio::awaitable<std::string> {
    // 从 rfl::Generic 转换为 Request 类型
    auto request_result = rfl::from_generic<Request, rfl::SnakeCaseToCamelCase>(params_generic);
    if (!request_result) {
      co_return create_error_response(id, ErrorCode::InvalidParams,
                                      "Invalid parameters: " + request_result.error().what());
    }

    // 执行业务逻辑 - 传递 app_state 参数
    auto result = co_await handler(app_state, request_result.value());

    // 构造响应
    if (result) {
      // 成功响应
      JsonRpcSuccessResponse success_response;
      success_response.id = id;
      success_response.result = rfl::to_generic<rfl::SnakeCaseToCamelCase>(result.value());
      co_return rfl::json::write<rfl::SnakeCaseToCamelCase>(success_response);
    } else {
      // 错误响应
      const auto& error = result.error();
      Logger().error("Error response: {}", error.message);
      co_return create_error_response(id, static_cast<ErrorCode>(error.code), error.message);
    }
  };

  // 存储到注册表
  registry[method_name] = MethodInfo{
      .name = method_name, .description = description, .handler = std::move(wrapped_handler)};
}

}  // namespace Core::RPC
