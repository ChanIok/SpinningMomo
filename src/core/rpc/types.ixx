module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

export module Core.RPC.Types;

import std;

export namespace Core::RPC {

// JSON-RPC 2.0 标准错误码
enum class ErrorCode {
  ParseError = -32700,      // JSON解析错误
  InvalidRequest = -32600,  // 无效请求
  MethodNotFound = -32601,  // 方法未找到
  InvalidParams = -32602,   // 无效参数
  InternalError = -32603,   // 内部错误
  ServerError = -32000      // 服务器错误
};

// RPC错误结构
struct RpcError {
  int code;
  std::string message;
  std::optional<std::string> data;  // 可选的详细信息
};

// 结果类型定义
template <typename T>
using RpcResult = std::expected<T, RpcError>;

struct MethodListItem {
  std::string name;
  std::string description;
};

// JSON-RPC请求结构
struct JsonRpcRequest {
  std::string jsonrpc;                 // 必须为 "2.0"
  std::string method;                  // 方法名
  std::optional<rfl::Generic> params;  // 参数(支持对象/数组/null)
  std::optional<rfl::Generic> id;      // 请求ID
};

// JSON-RPC成功响应结构
struct JsonRpcSuccessResponse {
  std::string jsonrpc{"2.0"};  // 版本号
  rfl::Generic result;         // 结果数据(JSON值)
  rfl::Generic id;             // 请求ID
};

// JSON-RPC错误响应结构
struct JsonRpcErrorResponse {
  std::string jsonrpc{"2.0"};  // 版本号
  RpcError error;              // 错误信息
  rfl::Generic id;             // 请求ID
};

// 方法信息存储结构
struct MethodInfo {
  std::string name;
  std::string description;
  std::function<asio::awaitable<std::string>(rfl::Generic, rfl::Generic)> handler;
};

}  // namespace Core::RPC
