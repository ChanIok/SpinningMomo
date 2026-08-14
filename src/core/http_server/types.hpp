#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/uwebsockets.hpp"

namespace core::http_server {

// ============= 流式传输配置 =============

// 流式传输阈值：超过此大小使用流式传输
constexpr size_t STREAM_THRESHOLD = 1024 * 1024;  // 1MB

// 流式传输块大小
constexpr size_t STREAM_CHUNK_SIZE = 65536;  // 64KB

// 流式传输上下文（完整状态）
struct StreamContext {
  // 文件相关
  asio::random_access_file file;
  std::filesystem::path file_path;
  size_t source_file_size;  // 完整文件大小（Content-Range 里的总长）
  size_t response_size;     // 本次 HTTP 体长度（uWS tryEnd 的 total；Range 时为片段字节数）
  size_t file_offset;       // 下一次 async_read 的起始绝对偏移
  size_t file_end_offset;   // 读到该偏移前停止（exclusive；即「尾字节 + 1」）

  // 响应相关
  std::string mime_type;
  std::string cache_control;
  std::string etag;
  std::string last_modified;
  size_t bytes_sent;
  int status_code = 200;
  std::optional<std::string> content_range_header;
  std::optional<std::string> content_disposition;
  bool accepts_ranges = true;  // 一次性归档关闭分段响应，确保完整传输后才能清理文件。

  // 运行时
  uWS::Loop* loop;
  uWS::HttpResponse<false>* res;
  std::vector<char> buffer;
  std::move_only_function<void()> on_complete;  // 完整响应结束后的资源清理回调。
  bool completion_called = false;               // 防止流式完成路径重复执行清理。

  // 状态
  std::shared_ptr<std::atomic_bool> abort_flag;
};

// 路径解析结果：成功时包含文件信息和缓存配置
struct PathResolutionData {
  std::filesystem::path file_path;
  std::optional<std::chrono::seconds> cache_duration;
  std::optional<std::string> cache_control_header;
};

using PathResolution = std::expected<PathResolutionData, std::string>;
using PathResolver = std::move_only_function<PathResolution(std::string_view) const>;

struct ResolverEntry {
  std::string prefix;
  PathResolver resolver;
};

// 路径解析器注册表
struct ResolverRegistry {
  std::vector<ResolverEntry> resolvers;
  // 请求并发读取，注册和注销独占写入；注销会等待正在执行的 resolver 结束。
  // resolver 执行期间不得注册或注销同一个注册表，避免共享锁升级死锁。
  std::shared_mutex mutex;
};

// SSE连接信息结构
struct SseConnection {
  uWS::HttpResponse<false>* response = nullptr;
  std::string client_id;
  std::chrono::system_clock::time_point connected_at;
  bool is_closed = false;
};

}  // namespace core::http_server
