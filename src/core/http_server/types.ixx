module;

#include <uwebsockets/App.h>
#include <asio.hpp>

export module Core.HttpServer.Types;

import std;

export namespace Core::HttpServer::Types {

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
  std::chrono::seconds cache_duration;
  size_t bytes_sent;
  int status_code = 200;
  std::optional<std::string> content_range_header;

  // 运行时
  uWS::Loop* loop;
  uWS::HttpResponse<false>* res;
  std::vector<char> buffer;

  // 状态
  bool is_aborted;
};

// 路径解析结果：成功时包含文件信息和缓存配置
struct PathResolutionData {
  std::filesystem::path file_path;
  std::optional<std::chrono::seconds> cache_duration;
};

using PathResolution = std::expected<PathResolutionData, std::string>;
using PathResolver = std::function<PathResolution(std::string_view)>;

struct ResolverEntry {
  std::string prefix;
  PathResolver resolver;
};

// 路径解析器注册表
struct ResolverRegistry {
  // 使用 atomic shared_ptr 实现无锁读取（RCU 模式）
  // 读取时无需加锁，写入时复制整个 vector
  std::atomic<std::shared_ptr<const std::vector<ResolverEntry>>> resolvers{
      std::make_shared<const std::vector<ResolverEntry>>()};

  // 写锁：仅用于保护写操作之间的竞争
  std::mutex write_mutex;
};

// SSE连接信息结构
struct SseConnection {
  uWS::HttpResponse<false>* response = nullptr;
  std::string client_id;
  std::chrono::system_clock::time_point connected_at;
  bool is_closed = false;
};

}  // namespace Core::HttpServer::Types
