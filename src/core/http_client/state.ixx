module;

export module Core.HttpClient.State;

import std;
import Core.HttpClient.Types;
import Vendor.WinHttp;
import <asio.hpp>;

export namespace Core::HttpClient::State {

// 单次异步 HTTP 请求的完整运行时上下文。对象由协程侧创建，通过 keepalive 自引用延长生命周期，
// 确保在 WinHTTP 后台回调结束前不被析构。
struct RequestOperation {
  // 自引用保活指针：在请求投递后置为自身，待操作彻底完成（包括句柄关闭回调）后清除。
  std::shared_ptr<RequestOperation> keepalive;
  std::mutex keepalive_mutex;

  Core::HttpClient::Types::Request request;
  Core::HttpClient::Types::Response response;
  // 最终结果：初始为"未完成"错误态，由 complete_operation / complete_with_error 写入。
  std::expected<Core::HttpClient::Types::Response, std::string> result =
      std::unexpected("Request is not completed");

  // 协程执行器与完成通知定时器：timer 到期即唤醒挂起的协程。
  asio::any_io_executor executor;
  std::optional<asio::steady_timer> completion_timer = std::nullopt;

  // WinHTTP 接口所需的 UTF-16 宽字符串，由 prepare_operation 在投递前填充。
  std::wstring wide_url;
  std::wstring wide_method;
  std::wstring wide_host;
  std::wstring wide_path;
  std::wstring wide_headers;

  std::vector<char> request_body;
  // 固定大小的读缓冲区，每次 WinHttpReadData 将数据写入此处。
  std::array<char, 16 * 1024> read_buffer{};

  // 文件下载专用选项。有值表示当前请求为文件下载模式，响应体将流式写入文件而非内存。
  struct DownloadOptions {
    std::filesystem::path output_path;
    std::optional<std::ofstream> output_file;
    std::uint64_t downloaded_bytes = 0;
    std::optional<std::uint64_t> total_bytes;  // 来自 Content-Length，服务器不保证提供
    Core::HttpClient::Types::DownloadProgressCallback progress_callback;
  };
  std::optional<DownloadOptions> download;

  Vendor::WinHttp::HINTERNET connect_handle = nullptr;
  Vendor::WinHttp::HINTERNET request_handle = nullptr;

  Vendor::WinHttp::INTERNET_PORT port = 0;
  bool secure = false;
  bool callback_registered = false;  // 已向 request_handle 注册状态回调
  bool receive_started = false;

  std::atomic<bool> completed{false};        // 操作是否已进入完成状态（结果已写入）
  std::atomic<bool> waiter_notified{false};  // 协程唤醒通知是否已发出（防止重复触发）
  std::atomic<bool> close_requested{false};  // WinHttpCloseHandle 是否已调用（防止重复关闭）
};

struct HttpClientState {
  Vendor::WinHttp::UniqueHInternet session;
  std::wstring user_agent = L"SpinningMomo/1.0";

  int resolve_timeout_ms = 0;
  int connect_timeout_ms = 10'000;
  int send_timeout_ms = 30'000;
  int receive_timeout_ms = 30'000;

  std::atomic<bool> is_initialized{false};
};

}  // namespace Core::HttpClient::State
