module;

#include <asio.hpp>
#include <rfl/json.hpp>

module Core.RPC;

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

// 获取所有已注册方法的列表
auto get_method_list(const Core::State::AppState& app_state) -> std::vector<MethodListItem> {
  std::vector<MethodListItem> methods;
  const auto& registry = app_state.rpc->registry;

  for (const auto& [name, info] : registry) {
    methods.emplace_back(MethodListItem{.name = name, .description = info.description});
  }

  return methods;
}

// 处理系统内置方法
auto handle_system_method(Core::State::AppState& app_state, const JsonRpcRequest& request,
                          rfl::Generic request_id) -> std::optional<std::string> {
  if (request.method == "system.listMethods") {
    JsonRpcSuccessResponse success_response;
    success_response.id = request_id;
    success_response.result = rfl::to_generic(get_method_list(app_state));
    return rfl::json::write<rfl::SnakeCaseToCamelCase>(success_response);
  }

  if (request.method == "system.methodSignature") {
    // 提前验证参数
    if (!request.params.has_value()) {
      return create_error_response(request_id, ErrorCode::InvalidParams,
                                   "Missing required parameter: method");
    }

    auto signature_request_result =
        rfl::from_generic<MethodSignatureRequest, rfl::SnakeCaseToCamelCase>(
            request.params.value());
    if (!signature_request_result) {
      return create_error_response(
          request_id, ErrorCode::InvalidParams,
          "Invalid parameters: " + signature_request_result.error().what());
    }

    const auto& registry = app_state.rpc->registry;
    const auto method_it = registry.find(signature_request_result.value().method);

    if (method_it == registry.end()) {
      return create_error_response(request_id, ErrorCode::MethodNotFound,
                                   "Method not found: " + signature_request_result.value().method);
    }

    // 构造响应
    MethodSignatureResponse signature_response{.method = method_it->second.name,
                                               .description = method_it->second.description,
                                               .params_schema = method_it->second.params_schema};

    JsonRpcSuccessResponse success_response;
    success_response.id = request_id;
    success_response.result = rfl::to_generic<rfl::SnakeCaseToCamelCase>(signature_response);
    return rfl::json::write<rfl::SnakeCaseToCamelCase>(success_response);
  }

  return std::nullopt;  // 不是系统方法
}

// 执行已注册的方法
auto execute_registered_method(const MethodInfo& method_info, rfl::Generic params_generic,
                               rfl::Generic request_id) -> asio::awaitable<std::string> {
  try {
    auto response_json = co_await method_info.handler(params_generic, request_id);
    Logger().trace("Response: {}", response_json);
    co_return response_json;
  } catch (const std::exception& e) {
    Logger().error("Internal error during method execution: {}", e.what());
    co_return create_error_response(
        request_id, ErrorCode::InternalError,
        "Internal error during method execution: " + std::string(e.what()));
  }
}

// 处理JSON-RPC 2.0协议请求 - 优化版本
auto process_request(Core::State::AppState& app_state, const std::string& request_json)
    -> asio::awaitable<std::string> {
  try {
    // 解析JSON-RPC请求
    auto request_result = rfl::json::read<JsonRpcRequest, rfl::SnakeCaseToCamelCase>(request_json);
    if (!request_result) {
      const auto error_msg = "Parse error: " + std::string(request_result.error().what());
      Logger().error(error_msg);
      co_return create_error_response(rfl::Generic(), ErrorCode::ParseError, error_msg);
    }

    auto request = request_result.value();
    const rfl::Generic request_id = request.id.value_or(rfl::Generic());

    // 验证JSON-RPC版本
    if (request.jsonrpc != "2.0") {
      const auto error_msg = "Invalid request: jsonrpc must be '2.0'";
      Logger().error(error_msg);
      co_return create_error_response(request_id, ErrorCode::InvalidRequest, error_msg);
    }

    // 处理系统内置方法
    if (auto system_response = handle_system_method(app_state, request, request_id)) {
      co_return system_response.value();
    }

    // 查找已注册的方法
    const auto& registry = app_state.rpc->registry;
    const auto method_it = registry.find(request.method);
    if (method_it == registry.end()) {
      const auto error_msg = "Method not found: " + request.method;
      Logger().error(error_msg);
      co_return create_error_response(request_id, ErrorCode::MethodNotFound, error_msg);
    }

    // 准备参数
    rfl::Generic params_generic = request.params.value_or(rfl::Generic::Object());

    // 执行方法处理器
    co_return co_await execute_registered_method(method_it->second, params_generic, request_id);

  } catch (const std::exception& e) {
    // 顶层异常处理
    const auto error_msg = "Unexpected error: " + std::string(e.what());
    Logger().error(error_msg);
    co_return create_error_response(rfl::Generic(), ErrorCode::InternalError, error_msg);
  }
}

}  // namespace Core::RPC
