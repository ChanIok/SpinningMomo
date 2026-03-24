module;

#include <asio.hpp>

module Core.HttpClient;

import std;
import Core.State;
import Core.HttpClient.State;
import Core.HttpClient.Types;
import Utils.Logger;
import Utils.String;
import Vendor.WinHttp;
import <windows.h>;

namespace Core::HttpClient::Detail {

using RequestOperation = Core::HttpClient::State::RequestOperation;

// 延长异步操作生命周期，确保在底层 WinHTTP 回调结束前对象不被析构
auto acquire_keepalive(RequestOperation* operation) -> std::shared_ptr<RequestOperation> {
  if (operation == nullptr) {
    return {};
  }

  std::lock_guard<std::mutex> lock(operation->keepalive_mutex);
  return operation->keepalive;
}

// 清除保活引用，允许异步操作对象在后续流程中被正常析构
auto release_keepalive(RequestOperation& operation) -> void {
  std::lock_guard<std::mutex> lock(operation.keepalive_mutex);
  operation.keepalive.reset();
}

// 生成带有 Windows 系统错误码的详细报告信息
auto make_winhttp_error(std::string_view stage) -> std::string {
  return std::format("{} failed (error={})", stage, ::GetLastError());
}

// 强制使超时定时器过期，以此来唤醒由于等待响应而挂起的协程
auto notify_waiter(RequestOperation& operation) -> void {
  if (operation.waiter_notified.exchange(true)) {
    return;
  }
  if (operation.completion_timer.has_value()) {
    operation.completion_timer->expires_at((std::chrono::steady_clock::time_point::min)());
  }
}

auto close_connect_handle(RequestOperation& operation) -> void {
  if (operation.connect_handle != nullptr) {
    Vendor::WinHttp::WinHttpCloseHandle(operation.connect_handle);
    operation.connect_handle = nullptr;
  }
}

// 关闭请求句柄（幂等）。若回调已注册，句柄置空延迟到 HANDLE_CLOSING 回调中处理，
// 避免在回调仍在运行时提前释放句柄导致悬空指针。
auto close_request_handle(RequestOperation& operation) -> void {
  if (operation.request_handle == nullptr) {
    return;
  }
  if (operation.close_requested.exchange(true)) {
    return;
  }
  Vendor::WinHttp::WinHttpCloseHandle(operation.request_handle);
  if (!operation.callback_registered) {
    operation.request_handle = nullptr;
  }
}

// 完成整个操作周期：保存结果、清理所有关联的 WinHTTP 句柄并唤醒等待的协程
auto complete_operation(std::shared_ptr<RequestOperation> operation,
                        std::expected<Types::Response, std::string> result) -> void {
  if (operation == nullptr) {
    return;
  }
  if (operation->completed.exchange(true)) {
    return;
  }

  operation->result = std::move(result);
  close_connect_handle(*operation);

  if (operation->request_handle != nullptr) {
    close_request_handle(*operation);
  } else {
    release_keepalive(*operation);
  }

  notify_waiter(*operation);
}

auto complete_with_error(std::shared_ptr<RequestOperation> operation, std::string message) -> void {
  complete_operation(std::move(operation), std::unexpected(std::move(message)));
}

// 将 UTF-8 编码的字符串转换为 WinHTTP 接口所需的宽字符串（UTF-16）
auto to_wide_utf8(const std::string& value, std::string_view field_name)
    -> std::expected<std::wstring, std::string> {
  auto wide = Utils::String::FromUtf8(value);
  if (!value.empty() && wide.empty()) {
    return std::unexpected(std::format("Invalid UTF-8 for {}", field_name));
  }
  return wide;
}

// HTTP 方法缺省为 GET，并统一转为大写
auto normalize_method(std::string method) -> std::string {
  if (method.empty()) {
    method = "GET";
  }
  std::ranges::transform(method, method.begin(),
                         [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return method;
}

auto trim_wstring(std::wstring_view value) -> std::wstring_view {
  auto is_space = [](wchar_t ch) { return std::iswspace(ch) != 0; };
  auto begin = std::find_if_not(value.begin(), value.end(), is_space);
  if (begin == value.end()) {
    return {};
  }
  auto end = std::find_if_not(value.rbegin(), value.rend(), is_space).base();
  return std::wstring_view(begin, end);
}

// 逐行解析 WinHTTP 返回的原始响应头字符串，过滤状态行并拆分键值对
auto parse_raw_headers(std::wstring_view raw_headers) -> std::vector<Types::Header> {
  std::vector<Types::Header> headers;

  size_t cursor = 0;
  bool skipped_status_line = false;
  while (cursor < raw_headers.size()) {
    auto line_end = raw_headers.find(L"\r\n", cursor);
    if (line_end == std::wstring_view::npos) {
      line_end = raw_headers.size();
    }

    auto line = raw_headers.substr(cursor, line_end - cursor);
    cursor = line_end + 2;

    if (line.empty()) {
      continue;
    }
    if (!skipped_status_line) {
      skipped_status_line = true;
      continue;
    }

    auto separator = line.find(L':');
    if (separator == std::wstring_view::npos) {
      continue;
    }

    auto name = trim_wstring(line.substr(0, separator));
    auto value = trim_wstring(line.substr(separator + 1));
    if (name.empty()) {
      continue;
    }

    headers.push_back(Types::Header{
        .name = Utils::String::ToUtf8(std::wstring(name)),
        .value = Utils::String::ToUtf8(std::wstring(value)),
    });
  }

  return headers;
}

// 从响应头中查找 Content-Length 并解析为无符号整数；服务器不保证提供，失败时返回 nullopt
auto find_content_length(const Types::Response& response) -> std::optional<std::uint64_t> {
  for (const auto& header : response.headers) {
    if (Utils::String::ToLowerAscii(header.name) != "content-length") {
      continue;
    }

    try {
      return static_cast<std::uint64_t>(std::stoull(Utils::String::TrimAscii(header.value)));
    } catch (...) {
      return std::nullopt;
    }
  }

  return std::nullopt;
}

// 向调用方发送当前下载进度快照；无回调或非文件下载模式时为空操作
auto emit_download_progress(RequestOperation& operation) -> void {
  if (!operation.download || !operation.download->progress_callback) {
    return;
  }

  operation.download->progress_callback(Types::DownloadProgress{
      .downloaded_bytes = operation.download->downloaded_bytes,
      .total_bytes = operation.download->total_bytes,
  });
}

// 刷新并关闭输出文件；非文件下载模式时为空操作
auto finalize_file_download(RequestOperation& operation) -> std::expected<void, std::string> {
  if (!operation.download || !operation.download->output_file.has_value()) {
    return {};
  }

  auto& dl = *operation.download;
  dl.output_file->flush();
  if (!dl.output_file->good()) {
    return std::unexpected("Failed to flush output file: " + dl.output_path.string());
  }

  dl.output_file->close();
  if (dl.output_file->fail()) {
    return std::unexpected("Failed to close output file: " + dl.output_path.string());
  }

  dl.output_file.reset();
  return {};
}

// 请求 URL 解析：将字符串分解为 host、path、port、scheme 等字段供后续 WinHTTP 调用使用
auto parse_request_url(RequestOperation& operation) -> std::expected<void, std::string> {
  auto wide_url_result = to_wide_utf8(operation.request.url, "request.url");
  if (!wide_url_result) {
    return std::unexpected(wide_url_result.error());
  }
  operation.wide_url = std::move(wide_url_result.value());

  Vendor::WinHttp::URL_COMPONENTS components{};
  components.dwStructSize = sizeof(components);
  components.dwSchemeLength = static_cast<Vendor::WinHttp::DWORD>(-1);
  components.dwHostNameLength = static_cast<Vendor::WinHttp::DWORD>(-1);
  components.dwUrlPathLength = static_cast<Vendor::WinHttp::DWORD>(-1);
  components.dwExtraInfoLength = static_cast<Vendor::WinHttp::DWORD>(-1);

  if (!Vendor::WinHttp::WinHttpCrackUrl(
          operation.wide_url.c_str(),
          static_cast<Vendor::WinHttp::DWORD>(operation.wide_url.size()), 0, &components)) {
    return std::unexpected(make_winhttp_error("WinHttpCrackUrl"));
  }

  if (components.dwHostNameLength == 0 || components.lpszHostName == nullptr) {
    return std::unexpected("Invalid URL host");
  }

  operation.wide_host.assign(components.lpszHostName, components.dwHostNameLength);

  operation.wide_path.clear();
  if (components.dwUrlPathLength > 0 && components.lpszUrlPath != nullptr) {
    operation.wide_path.append(components.lpszUrlPath, components.dwUrlPathLength);
  }
  if (components.dwExtraInfoLength > 0 && components.lpszExtraInfo != nullptr) {
    operation.wide_path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
  }
  if (operation.wide_path.empty()) {
    operation.wide_path = L"/";
  }

  operation.port = components.nPort;
  operation.secure = components.nScheme == Vendor::WinHttp::kINTERNET_SCHEME_HTTPS;
  return {};
}

// 将请求头列表序列化为 WinHTTP 所需的宽字符串格式（"Name: Value\r\n" 逐行拼接）
auto build_request_headers(RequestOperation& operation) -> std::expected<void, std::string> {
  operation.wide_headers.clear();

  for (const auto& header : operation.request.headers) {
    if (header.name.empty()) {
      continue;
    }

    auto wide_name_result = to_wide_utf8(header.name, "request.headers.name");
    if (!wide_name_result) {
      return std::unexpected(wide_name_result.error());
    }
    auto wide_value_result = to_wide_utf8(header.value, "request.headers.value");
    if (!wide_value_result) {
      return std::unexpected(wide_value_result.error());
    }

    operation.wide_headers += wide_name_result.value();
    operation.wide_headers += L": ";
    operation.wide_headers += wide_value_result.value();
    operation.wide_headers += L"\r\n";
  }

  return {};
}

// 从 WinHTTP 句柄中读取 HTTP 状态码（如 200、404）并写入 response
auto query_response_status(RequestOperation& operation) -> std::expected<void, std::string> {
  Vendor::WinHttp::DWORD status_code = 0;
  Vendor::WinHttp::DWORD status_size = sizeof(status_code);

  if (!Vendor::WinHttp::WinHttpQueryHeaders(
          operation.request_handle,
          Vendor::WinHttp::kWINHTTP_QUERY_STATUS_CODE | Vendor::WinHttp::kWINHTTP_QUERY_FLAG_NUMBER,
          Vendor::WinHttp::kWINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, nullptr)) {
    return std::unexpected(make_winhttp_error("WinHttpQueryHeaders(status)"));
  }

  operation.response.status_code = static_cast<std::int32_t>(status_code);
  return {};
}

// 查询并解析完整响应头列表，解析失败时静默跳过（状态码已单独提取）
auto query_response_headers(RequestOperation& operation) -> void {
  Vendor::WinHttp::DWORD header_size = 0;
  (void)Vendor::WinHttp::WinHttpQueryHeaders(
      operation.request_handle, Vendor::WinHttp::kWINHTTP_QUERY_RAW_HEADERS_CRLF,
      Vendor::WinHttp::kWINHTTP_HEADER_NAME_BY_INDEX, nullptr, &header_size, nullptr);
  if (header_size == 0) {
    return;
  }

  std::wstring raw_headers(header_size / sizeof(wchar_t), L'\0');
  if (!Vendor::WinHttp::WinHttpQueryHeaders(operation.request_handle,
                                            Vendor::WinHttp::kWINHTTP_QUERY_RAW_HEADERS_CRLF,
                                            Vendor::WinHttp::kWINHTTP_HEADER_NAME_BY_INDEX,
                                            raw_headers.data(), &header_size, nullptr)) {
    return;
  }

  if (!raw_headers.empty() && raw_headers.back() == L'\0') {
    raw_headers.pop_back();
  }
  operation.response.headers = parse_raw_headers(raw_headers);
}

// 向 WinHTTP 查询当前可读字节数，触发后续 DATA_AVAILABLE 回调
auto request_more_data(std::shared_ptr<RequestOperation> operation)
    -> std::expected<void, std::string> {
  if (!Vendor::WinHttp::WinHttpQueryDataAvailable(operation->request_handle, nullptr)) {
    return std::unexpected(make_winhttp_error("WinHttpQueryDataAvailable"));
  }
  return {};
}

// 下载结束收尾：刷新关闭文件、发送最终进度通知、标记操作完成
auto complete_download(std::shared_ptr<RequestOperation> operation) -> void {
  auto finalize_result = finalize_file_download(*operation);
  if (!finalize_result) {
    complete_with_error(operation, finalize_result.error());
    return;
  }
  emit_download_progress(*operation);
  complete_operation(operation, operation->response);
}

// 将回调逻辑派发到协程执行器线程，避免在 WinHTTP 系统线程上直接操作 operation 状态
template <typename F>
auto post_status_callback(std::shared_ptr<RequestOperation> operation, F&& fn) -> void {
  asio::post(operation->executor, [operation, fn = std::forward<F>(fn)]() mutable { fn(); });
}

// WinHTTP 核心异步回调函数：由系统底层触发，负责处理连接、收发数据等不同阶段的状态变更
auto CALLBACK winhttp_status_callback(Vendor::WinHttp::HINTERNET h_internet,
                                      Vendor::WinHttp::DWORD_PTR context,
                                      Vendor::WinHttp::DWORD internet_status,
                                      Vendor::WinHttp::LPVOID status_information,
                                      Vendor::WinHttp::DWORD status_information_length) -> void {
  auto* raw_operation = reinterpret_cast<RequestOperation*>(context);
  auto operation = acquire_keepalive(raw_operation);
  if (!operation) {
    return;
  }

  switch (internet_status) {
    case Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE: {
      post_status_callback(operation, [operation]() mutable {
        if (operation->completed.load()) {
          return;
        }
        // 请求发送完毕，开始等待并接收服务器的响应
        if (!Vendor::WinHttp::WinHttpReceiveResponse(operation->request_handle, nullptr)) {
          complete_with_error(operation, make_winhttp_error("WinHttpReceiveResponse"));
        }
      });
      break;
    }
    case Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: {
      post_status_callback(operation, [operation]() mutable {
        if (operation->completed.load()) {
          return;
        }

        // 响应头已可用，先提取 HTTP 状态码 (例如 200, 404)
        auto status_result = query_response_status(*operation);
        if (!status_result) {
          complete_with_error(operation, status_result.error());
          return;
        }

        // 继续提取所有响应头并解析
        query_response_headers(*operation);
        if (operation->download) {
          operation->download->total_bytes = find_content_length(operation->response);
        }
        // 尝试查询是否有可用的响应体数据
        auto query_result = request_more_data(operation);
        if (!query_result) {
          complete_with_error(operation, query_result.error());
        }
      });
      break;
    }
    case Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
      auto available_bytes = *reinterpret_cast<Vendor::WinHttp::DWORD*>(status_information);

      post_status_callback(operation, [operation, available_bytes]() mutable {
        if (operation->completed.load()) {
          return;
        }
        // 若可用数据为 0，说明响应体已经彻底接收完毕
        if (available_bytes == 0) {
          complete_download(operation);
          return;
        }

        // 发起异步读取操作，将数据读入预分配的内部 buffer 中
        auto bytes_to_read = static_cast<Vendor::WinHttp::DWORD>(
            std::min<std::size_t>(available_bytes, operation->read_buffer.size()));

        if (!Vendor::WinHttp::WinHttpReadData(
                operation->request_handle, operation->read_buffer.data(), bytes_to_read, nullptr)) {
          complete_with_error(operation, make_winhttp_error("WinHttpReadData"));
        }
      });
      break;
    }
    case Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
      auto bytes_read = static_cast<std::size_t>(status_information_length);
      post_status_callback(operation, [operation, bytes_read]() mutable {
        if (operation->completed.load()) {
          return;
        }
        // 如果读取完成但读取到的字节为0，可能对端提前关闭，同样视为结束
        if (bytes_read == 0) {
          complete_download(operation);
          return;
        }

        if (operation->download) {
          if (!operation->download->output_file.has_value()) {
            complete_with_error(operation, "Output file is not initialized: " +
                                               operation->download->output_path.string());
            return;
          }

          operation->download->output_file->write(operation->read_buffer.data(),
                                                  static_cast<std::streamsize>(bytes_read));
          if (!operation->download->output_file->good()) {
            complete_with_error(operation, "Failed to write output file: " +
                                               operation->download->output_path.string());
            return;
          }

          operation->download->downloaded_bytes += bytes_read;
          emit_download_progress(*operation);
        } else {
          // 把刚读到的数据追加到总的 response body 里
          operation->response.body.append(operation->read_buffer.data(), bytes_read);
        }
        // 循环继续询问还有没有剩余的流数据
        auto query_result = request_more_data(operation);
        if (!query_result) {
          complete_with_error(operation, query_result.error());
        }
      });
      break;
    }
    case Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
      auto async_result =
          *reinterpret_cast<Vendor::WinHttp::WINHTTP_ASYNC_RESULT*>(status_information);

      post_status_callback(operation, [operation, async_result]() mutable {
        if (operation->completed.load()) {
          return;
        }
        complete_with_error(operation, std::format("WinHTTP async request error (api={}, error={})",
                                                   async_result.dwResult, async_result.dwError));
      });
      break;
    }
    case Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_HANDLE_CLOSING: {
      post_status_callback(operation, [operation, h_internet]() mutable {
        if (operation->request_handle == h_internet) {
          operation->request_handle = nullptr;
          operation->callback_registered = false;
        }
        // 只有确信 request_handle 被完全关闭且操作收尾后，才释放保活引用
        if (operation->completed.load()) {
          release_keepalive(*operation);
        }
      });
      break;
    }
    default:
      break;
  }
}

// 解析请求参数、创建且配置相应的 WinHTTP 连接和请求句柄，绑定异步回调后开始发送。
// 若此函数返回错误，说明请求完全未进入系统队列，调用方需自行释放 keepalive。
auto prepare_operation(State::HttpClientState& state, std::shared_ptr<RequestOperation> operation)
    -> std::expected<void, std::string> {
  operation->request.method = normalize_method(operation->request.method);
  operation->request_body.assign(operation->request.body.begin(), operation->request.body.end());

  auto method_result = to_wide_utf8(operation->request.method, "request.method");
  if (!method_result) {
    return std::unexpected(method_result.error());
  }
  operation->wide_method = std::move(method_result.value());

  if (auto parse_result = parse_request_url(*operation); !parse_result) {
    return std::unexpected(parse_result.error());
  }
  if (auto header_result = build_request_headers(*operation); !header_result) {
    return std::unexpected(header_result.error());
  }

  // 第一步：创建一个到目标主机端口的连接 (Connect)
  operation->connect_handle = Vendor::WinHttp::WinHttpConnect(
      state.session.get(), operation->wide_host.c_str(), operation->port, 0);
  if (operation->connect_handle == nullptr) {
    return std::unexpected(make_winhttp_error("WinHttpConnect"));
  }

  // 第二步：利用上面建立的连接去初始化一个特定 URI 的请求句柄 (Request)
  Vendor::WinHttp::DWORD request_flags =
      operation->secure ? Vendor::WinHttp::kWINHTTP_FLAG_SECURE : 0;
  operation->request_handle = Vendor::WinHttp::WinHttpOpenRequest(
      operation->connect_handle, operation->wide_method.c_str(), operation->wide_path.c_str(),
      nullptr, Vendor::WinHttp::kWINHTTP_NO_REFERER, Vendor::WinHttp::kWINHTTP_DEFAULT_ACCEPT_TYPES,
      request_flags);
  if (operation->request_handle == nullptr) {
    close_connect_handle(*operation);
    return std::unexpected(make_winhttp_error("WinHttpOpenRequest"));
  }

  int connect_timeout = operation->request.connect_timeout_ms.value_or(state.connect_timeout_ms);
  int send_timeout = operation->request.send_timeout_ms.value_or(state.send_timeout_ms);
  int receive_timeout = operation->request.receive_timeout_ms.value_or(state.receive_timeout_ms);
  if (!Vendor::WinHttp::WinHttpSetTimeouts(operation->request_handle, state.resolve_timeout_ms,
                                           connect_timeout, send_timeout, receive_timeout)) {
    close_request_handle(*operation);
    close_connect_handle(*operation);
    return std::unexpected(make_winhttp_error("WinHttpSetTimeouts"));
  }

  // 绑定上下文：非常关键，将 operation 指针与该请求句柄关联，后续 WinHTTP
  // 回调才能拿到我们的操作上下文
  Vendor::WinHttp::DWORD_PTR context =
      reinterpret_cast<Vendor::WinHttp::DWORD_PTR>(operation.get());
  if (!Vendor::WinHttp::WinHttpSetOption(operation->request_handle,
                                         Vendor::WinHttp::kWINHTTP_OPTION_CONTEXT_VALUE, &context,
                                         sizeof(context))) {
    close_request_handle(*operation);
    close_connect_handle(*operation);
    return std::unexpected(make_winhttp_error("WinHttpSetOption(context)"));
  }

  // 设置我们感兴趣的 WinHTTP 异步回调阶段，并挂载 winhttp_status_callback
  auto callback_flags = Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE |
                        Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE |
                        Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_DATA_AVAILABLE |
                        Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_READ_COMPLETE |
                        Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_REQUEST_ERROR |
                        Vendor::WinHttp::kWINHTTP_CALLBACK_STATUS_HANDLE_CLOSING;
  auto callback_result = Vendor::WinHttp::WinHttpSetStatusCallback(
      operation->request_handle, winhttp_status_callback, callback_flags, 0);
  if (callback_result == Vendor::WinHttp::kWINHTTP_INVALID_STATUS_CALLBACK) {
    close_request_handle(*operation);
    close_connect_handle(*operation);
    return std::unexpected(make_winhttp_error("WinHttpSetStatusCallback"));
  }
  operation->callback_registered = true;

  const wchar_t* header_ptr = operation->wide_headers.empty()
                                  ? Vendor::WinHttp::kWINHTTP_NO_ADDITIONAL_HEADERS
                                  : operation->wide_headers.c_str();
  auto header_len = operation->wide_headers.empty()
                        ? 0
                        : static_cast<Vendor::WinHttp::DWORD>(operation->wide_headers.size());

  auto body_size = static_cast<Vendor::WinHttp::DWORD>(operation->request_body.size());
  void* request_data =
      body_size == 0 ? Vendor::WinHttp::kWINHTTP_NO_REQUEST_DATA : operation->request_body.data();

  // 第三步：将请求头发往服务器，由于设定了 ASYNC 标志，此函数会立刻返回，后续流程交由系统回调处理
  if (!Vendor::WinHttp::WinHttpSendRequest(operation->request_handle, header_ptr, header_len,
                                           request_data, body_size, body_size, 0)) {
    complete_with_error(operation, make_winhttp_error("WinHttpSendRequest"));
  }

  return {};
}

// 通用 operation 执行：设保活、投递至 WinHTTP、挂起协程直至完成或中断。
// fetch 和 download_to_file 均通过此函数统一驱动请求生命周期。
auto execute_operation(State::HttpClientState& client_state,
                       std::shared_ptr<RequestOperation> operation) -> asio::awaitable<void> {
  {
    // 自引用保活：防止局部运行完后 shared_ptr 被回收导致在 WinHTTP 后台回调里出现空指针
    std::lock_guard<std::mutex> lock(operation->keepalive_mutex);
    operation->keepalive = operation;
  }

  // 投递进入 WinHTTP：如果这一步失败，说明完全没丢进系统队列，需直接移除保活引用并返回
  if (auto prepare_result = prepare_operation(client_state, operation); !prepare_result) {
    release_keepalive(*operation);
    operation->result = std::unexpected(prepare_result.error());
    co_return;
  }

  if (operation->completed.load()) {
    co_return;
  }

  // 让当前的协程(coroutine)在此挂起，直到底层完成全部网络通讯唤醒此 timer
  std::error_code wait_error;
  co_await operation->completion_timer->async_wait(
      asio::redirect_error(asio::use_awaitable, wait_error));

  if (!operation->completed.load()) {
    complete_with_error(operation, "HTTP request interrupted before completion");
  }
}

}  // namespace Core::HttpClient::Detail

namespace Core::HttpClient {

// 初始化 HTTP 客户端全局状态，使用系统的自适应代理配置创建 WinHTTP 会话 (Session)
auto initialize(Core::State::AppState& state) -> std::expected<void, std::string> {
  if (!state.http_client) {
    return std::unexpected("HTTP client state is not initialized");
  }

  if (state.http_client->is_initialized.load()) {
    return {};
  }

  state.http_client->session = Vendor::WinHttp::UniqueHInternet{Vendor::WinHttp::WinHttpOpen(
      state.http_client->user_agent.c_str(), Vendor::WinHttp::kWINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      Vendor::WinHttp::kWINHTTP_NO_PROXY_NAME, Vendor::WinHttp::kWINHTTP_NO_PROXY_BYPASS,
      Vendor::WinHttp::kWINHTTP_FLAG_ASYNC)};
  if (!state.http_client->session) {
    return std::unexpected("Failed to open WinHTTP async session");
  }

  state.http_client->is_initialized = true;
  Logger().info("HTTP client initialized");
  return {};
}

// 关闭 HTTP 会话释放内核资源，后续请求将无法被挂起执行
auto shutdown(Core::State::AppState& state) -> void {
  if (!state.http_client) {
    return;
  }
  state.http_client->is_initialized = false;
  state.http_client->session = Vendor::WinHttp::UniqueHInternet{};
  Logger().info("HTTP client shut down");
}

// 核心异步 HTTP 请求：负责封装请求上下文，投递至 WinHTTP 进行处理并挂起当前协程直至完成
auto fetch(Core::State::AppState& state, const Core::HttpClient::Types::Request& request)
    -> asio::awaitable<std::expected<Core::HttpClient::Types::Response, std::string>> {
  if (!state.http_client) {
    co_return std::unexpected("HTTP client state is not initialized");
  }
  if (!state.http_client->is_initialized.load() || !state.http_client->session) {
    co_return std::unexpected("HTTP client is not initialized");
  }
  if (request.url.empty()) {
    co_return std::unexpected("HTTP request URL is empty");
  }

  auto executor = co_await asio::this_coro::executor;

  auto operation = std::make_shared<Core::HttpClient::State::RequestOperation>();
  operation->executor = executor;
  operation->completion_timer.emplace(executor);
  operation->completion_timer->expires_at((std::chrono::steady_clock::time_point::max)());
  operation->request = request;

  co_await Detail::execute_operation(*state.http_client, operation);
  co_return operation->result;
}

// 便捷封装：执行 HTTP 抓取请求后，将接收到的响应体直接以二进制流写入本地文件中
auto download_to_file(Core::State::AppState& state, const Core::HttpClient::Types::Request& request,
                      const std::filesystem::path& output_path,
                      Core::HttpClient::Types::DownloadProgressCallback progress_callback)
    -> asio::awaitable<std::expected<void, std::string>> {
  if (!state.http_client) {
    co_return std::unexpected("HTTP client state is not initialized");
  }
  if (!state.http_client->is_initialized.load() || !state.http_client->session) {
    co_return std::unexpected("HTTP client is not initialized");
  }
  if (request.url.empty()) {
    co_return std::unexpected("HTTP request URL is empty");
  }

  auto executor = co_await asio::this_coro::executor;

  auto operation = std::make_shared<Core::HttpClient::State::RequestOperation>();
  operation->executor = executor;
  operation->completion_timer.emplace(executor);
  operation->completion_timer->expires_at((std::chrono::steady_clock::time_point::max)());
  operation->request = request;

  auto& dl = operation->download.emplace();
  dl.output_path = output_path;
  dl.progress_callback = std::move(progress_callback);
  dl.output_file.emplace(output_path, std::ios::binary | std::ios::trunc);
  if (!dl.output_file->is_open()) {
    co_return std::unexpected("Failed to open output file: " + output_path.string());
  }

  co_await Detail::execute_operation(*state.http_client, operation);

  if (!operation->result) {
    operation->download->output_file.reset();
    co_return std::unexpected(operation->result.error());
  }

  if (operation->response.status_code != 200) {
    co_return std::unexpected("HTTP error: " + std::to_string(operation->response.status_code));
  }

  co_return std::expected<void, std::string>{};
}

}  // namespace Core::HttpClient
