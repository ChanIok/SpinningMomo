module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Engine;

import std;
import Core.State;
import Core.RPC.State;
import Core.RPC.Types;
import Utils.Logger;

namespace Core::RPC {

// 创建标准错误响应
auto create_error_response(rfl::Generic request_id, ErrorCode error_code,
                           const std::string& message) -> std::string {
  JsonRpcErrorResponse error_response;
  error_response.id = request_id;
  error_response.error = RpcError{.code = static_cast<int>(error_code), .message = message};
  return rfl::json::write<rfl::SnakeCaseToCamelCase>(error_response);
}

// 检查指定方法是否已注册
auto method_exists(const Core::State::AppState& app_state, const std::string& method_name) -> bool {
  return app_state.rpc->registry.find(method_name) != app_state.rpc->registry.end();
}

// 获取所有已注册方法的列表
auto get_method_list(const Core::State::AppState& app_state) -> std::vector<MethodListItem> {
  std::vector<MethodListItem> methods;
  const auto& registry = app_state.rpc->registry;

  for (const auto& [name, info] : registry) {
    methods.emplace_back(MethodListItem{.name = name, .description = info.description});
  }

  return methods;
}

// 处理JSON-RPC 2.0协议请求
auto process_request(Core::State::AppState& app_state, const std::string& request_json)
    -> asio::awaitable<std::string> {
  try {
    // 解析JSON-RPC请求
    auto request_result = rfl::json::read<JsonRpcRequest, rfl::SnakeCaseToCamelCase>(request_json);
    if (!request_result) {
      Logger().error("Parse error: {}", request_result.error().what());
      co_return create_error_response(rfl::Generic(), ErrorCode::ParseError,
                                      "Parse error: " + request_result.error().what());
    }

    auto request = request_result.value();
    rfl::Generic request_id = request.id.value_or(rfl::Generic());

    // 验证JSON-RPC版本
    if (request.jsonrpc != "2.0") {
      Logger().error("Invalid request: jsonrpc must be '2.0'");
      co_return create_error_response(request_id, ErrorCode::InvalidRequest,
                                      "Invalid request: jsonrpc must be '2.0'");
    }

    // 处理系统内置方法
    if (request.method == "system.listMethods") {
      JsonRpcSuccessResponse success_response;
      success_response.id = request_id;
      success_response.result = rfl::to_generic(get_method_list(app_state));
      co_return rfl::json::write<rfl::SnakeCaseToCamelCase>(success_response);
    }

    // 查找已注册的方法
    auto& registry = app_state.rpc->registry;
    auto method_it = registry.find(request.method);
    if (method_it == registry.end()) {
      Logger().error("Method not found: {}", request.method);
      co_return create_error_response(request_id, ErrorCode::MethodNotFound,
                                      "Method not found: " + request.method);
    }

    // 执行方法处理器
    rfl::Generic params_generic = rfl::Generic::Object();
    if (request.params.has_value()) {
      params_generic = request.params.value();
    }

    try {
      auto response_json = co_await method_it->second.handler(params_generic, request_id);
      Logger().trace("Response: {}", response_json);
      co_return response_json;
    } catch (const std::exception& e) {
      Logger().error("Internal error during method execution: {}", e.what());
      co_return create_error_response(
          request_id, ErrorCode::InternalError,
          "Internal error during method execution: " + std::string(e.what()));
    }

  } catch (const std::exception& e) {
    // 顶层异常处理
    Logger().error("Unexpected error: {}", e.what());
    co_return create_error_response(rfl::Generic(), ErrorCode::InternalError,
                                    "Unexpected error: " + std::string(e.what()));
  }
}

}  // namespace Core::RPC
