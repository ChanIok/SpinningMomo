module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

export module Core.RpcHandlers;

import std;
import Core.State;
export import Core.RpcHandlers.Types;

namespace Core::RpcHandlers {

// 注册RPC方法
export template <typename Request, typename Response>
auto register_method(Core::State::AppState& app_state, const std::string& method_name,
                     AsyncHandler<Request, Response> handler, const std::string& description = "")
    -> void;

// 处理JSON-RPC请求
export auto process_request(Core::State::AppState& app_state, const std::string& request_json)
    -> asio::awaitable<std::string>;

// 获取已注册方法列表
export auto get_method_list(const Core::State::AppState& app_state) -> std::vector<MethodListItem>;

// 检查方法是否存在
export auto method_exists(const Core::State::AppState& app_state, const std::string& method_name)
    -> bool;

// 注册方法
template <typename Request, typename Response>
auto register_method(Core::State::AppState& app_state, const std::string& method_name,
                     AsyncHandler<Request, Response> handler, const std::string& description)
    -> void {
  // 创建类型擦除的处理器包装
  auto wrapped_handler = [handler](rfl::Generic params_generic,
                                   rfl::Generic id) -> asio::awaitable<std::string> {
    try {
      // 从 rfl::Generic 转换为 Request 类型
      auto request_result = rfl::from_generic<Request>(params_generic);
      if (!request_result) {
        JsonRpcErrorResponse error_response;
        error_response.id = id;
        error_response.error =
            RpcError{.code = static_cast<int>(ErrorCode::InvalidParams),
                     .message = "Invalid parameters: " + request_result.error().what()};
        co_return rfl::json::write(error_response);
      }

      // 执行业务逻辑
      auto result = co_await handler(request_result.value());

      // 构造响应
      if (result) {
        // 成功响应
        JsonRpcSuccessResponse success_response;
        success_response.id = id;
        success_response.result = rfl::to_generic(result.value());
        co_return rfl::json::write(success_response);
      } else {
        // 错误响应
        JsonRpcErrorResponse error_response;
        error_response.id = id;
        error_response.error = result.error();
        co_return rfl::json::write(error_response);
      }

    } catch (const std::exception& e) {
      // 异常处理
      JsonRpcErrorResponse error_response;
      error_response.id = id;
      error_response.error = RpcError{.code = static_cast<int>(ErrorCode::InternalError),
                                      .message = "Internal error: " + std::string(e.what())};
      co_return rfl::json::write(error_response);
    }
  };

  // 存储到注册表
  app_state.rpc_handlers.registry[method_name] = MethodInfo{
      .name = method_name, .description = description, .handler = std::move(wrapped_handler)};
}

}  // namespace Core::RpcHandlers