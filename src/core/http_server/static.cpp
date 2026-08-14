#include "core/http_server/static.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/uwebsockets.hpp"
#include "vendor/windows/fileapi.hpp"

#include "core/async/async.hpp"
#include "core/http_server/access.hpp"
#include "core/http_server/types.hpp"
#include "core/state/app_state.hpp"
#include "utils/file/file.hpp"
#include "utils/file/mime.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"

namespace core::http_server::static_content {

// 注册 HTTP 路径解析器：独占修改注册表并转移 resolver 所有权
auto register_path_resolver(core::AppState& state, std::string prefix, PathResolver resolver)
    -> void {
  if (!state.http_server) {
    Logger().error("HttpServer state not initialized, cannot register path resolver");
    return;
  }

  auto& registry = state.http_server->path_resolvers;
  std::unique_lock lock(registry.mutex);

  // 注册表独占 resolver，读取线程后续只通过 const 引用重复调用
  registry.resolvers.push_back({std::move(prefix), std::move(resolver)});
  Logger().debug("Registered custom path resolver for: {}", registry.resolvers.back().prefix);
}

// 注销指定前缀的 HTTP 路径解析器，并等待正在执行的读取离开共享区
auto unregister_path_resolver(core::AppState& state, std::string_view prefix) -> void {
  if (!state.http_server) {
    return;
  }

  auto& registry = state.http_server->path_resolvers;
  std::unique_lock lock(registry.mutex);

  // 独占锁同时充当生命周期屏障，返回后不会再有旧 resolver 正在执行
  std::erase_if(registry.resolvers, [prefix](const auto& entry) { return entry.prefix == prefix; });
  Logger().debug("Unregistered path resolver for: {}", prefix);
}

// 按注册顺序查找并调用首个能够解析当前 URL 的 HTTP resolver
auto try_custom_resolve(core::AppState& state, std::string_view url_path)
    -> std::optional<PathResolution> {
  if (!state.http_server) {
    return std::nullopt;
  }

  auto& registry = state.http_server->path_resolvers;
  // resolver 执行期间保持共享锁，让注销能够可靠等待在途请求结束
  std::shared_lock lock(registry.mutex);

  for (const auto& entry : registry.resolvers) {
    if (url_path.starts_with(entry.prefix)) {
      auto result = entry.resolver(url_path);
      if (result.has_value()) {
        return result;
      }
    }
  }
  return std::nullopt;
}

// 获取针对不同文件类型的缓存时间
auto get_cache_duration(std::string_view url_path, const std::filesystem::path& file_path)
    -> std::chrono::seconds {
  // Vite 产物使用内容哈希命名；版本变化会产生新文件名，可以长期缓存。
  if (url_path.starts_with("/assets/")) return std::chrono::seconds{31536000};

  auto extension = file_path.extension().string();

  // HTML文件：短缓存，便于开发调试
  if (extension == ".html") return std::chrono::seconds{60};

  // CSS/JS：中等缓存
  if (extension == ".css" || extension == ".js") return std::chrono::seconds{300};

  // 图片/字体：长缓存
  if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".svg" ||
      extension == ".woff" || extension == ".woff2" || extension == ".webp") {
    return std::chrono::seconds{3600};
  }

  // 默认：短缓存
  return std::chrono::seconds{180};
}

// 路径解析
auto resolve_file_path(const std::filesystem::path& web_root, const std::string& url_path)
    -> std::filesystem::path {
  auto clean_path = url_path == "/" ? "/index.html" : url_path;
  if (clean_path.ends_with("/")) clean_path += "index.html";

  return web_root / clean_path.substr(1);  // 移除开头的'/'
}

// 获取web根目录
auto get_web_root() -> std::filesystem::path {
  return utils::path::GetEmbeddedWebRootDirectory().value_or(".");
}

// ---- Range 请求：<video> 拖动进度、分片加载依赖 Accept-Ranges + 206 + Content-Range ----
struct ByteRange {
  size_t start = 0;
  size_t end = 0;  // inclusive
};

struct RangeHeaderParseResult {
  bool valid = true;
  std::optional<ByteRange> range;
};

auto parse_range_header(std::string_view header_value, size_t file_size) -> RangeHeaderParseResult {
  if (header_value.empty()) {
    return {};
  }

  // V1 仅支持单一 byte range，已经足够覆盖浏览器 / <video> 的 seek 场景。
  if (!header_value.starts_with("bytes=") || file_size == 0) {
    return {.valid = false, .range = std::nullopt};
  }

  auto range_spec = header_value.substr(6);
  auto comma_pos = range_spec.find(',');
  if (comma_pos != std::string_view::npos) {
    return {.valid = false, .range = std::nullopt};
  }

  auto dash_pos = range_spec.find('-');
  if (dash_pos == std::string_view::npos) {
    return {.valid = false, .range = std::nullopt};
  }

  auto start_part = range_spec.substr(0, dash_pos);
  auto end_part = range_spec.substr(dash_pos + 1);

  if (start_part.empty()) {
    size_t suffix_length = 0;
    auto [ptr, ec] =
        std::from_chars(end_part.data(), end_part.data() + end_part.size(), suffix_length);
    if (ec != std::errc{} || ptr != end_part.data() + end_part.size() || suffix_length == 0) {
      return {.valid = false, .range = std::nullopt};
    }

    size_t clamped_length = std::min(suffix_length, file_size);
    return {.valid = true,
            .range = ByteRange{.start = file_size - clamped_length, .end = file_size - 1}};
  }

  size_t start = 0;
  auto [start_ptr, start_ec] =
      std::from_chars(start_part.data(), start_part.data() + start_part.size(), start);
  if (start_ec != std::errc{} || start_ptr != start_part.data() + start_part.size() ||
      start >= file_size) {
    return {.valid = false, .range = std::nullopt};
  }

  if (end_part.empty()) {
    return {.valid = true, .range = ByteRange{.start = start, .end = file_size - 1}};
  }

  size_t end = 0;
  auto [end_ptr, end_ec] = std::from_chars(end_part.data(), end_part.data() + end_part.size(), end);
  if (end_ec != std::errc{} || end_ptr != end_part.data() + end_part.size() || end < start) {
    return {.valid = false, .range = std::nullopt};
  }

  return {.valid = true, .range = ByteRange{.start = start, .end = std::min(end, file_size - 1)}};
}

auto get_response_content_type(const std::string& mime_type) -> std::string {
  if (mime_type.starts_with("text/") && !mime_type.contains("charset=")) {
    return mime_type + "; charset=utf-8";
  }

  return mime_type;
}

struct CacheValidators {
  std::string etag;
  std::string last_modified;
};

struct FileMetadata {
  size_t size = 0;
  std::int64_t modified_seconds = 0;
  std::chrono::system_clock::time_point modified_time{};
};

// 一次读取文件属性，复用文件大小和最后修改时间，避免多个 filesystem 查询。
auto query_file_metadata(const std::filesystem::path& file_path)
    -> std::expected<FileMetadata, std::string> {
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  if (!GetFileAttributesExW(file_path.c_str(), GetFileExInfoStandard, &attributes)) {
    return std::unexpected(std::format("GetFileAttributesExW failed with Win32 error {}",
                                       static_cast<unsigned long>(GetLastError())));
  }

  if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    return std::unexpected("Resolved path is a directory");
  }

  ULARGE_INTEGER size{};
  size.LowPart = attributes.nFileSizeLow;
  size.HighPart = attributes.nFileSizeHigh;
  if (size.QuadPart > std::numeric_limits<size_t>::max()) {
    return std::unexpected("Resolved file is too large for this platform");
  }

  ULARGE_INTEGER modified_time{};
  modified_time.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
  modified_time.HighPart = attributes.ftLastWriteTime.dwHighDateTime;

  // Windows FILETIME uses 100ns ticks since 1601-01-01; HTTP dates use Unix seconds.
  constexpr std::uint64_t kWindowsToUnixEpoch100ns = 116444736000000000ULL;
  if (modified_time.QuadPart < kWindowsToUnixEpoch100ns) {
    return std::unexpected("Resolved file has an invalid last write time");
  }

  const auto modified_seconds =
      static_cast<std::int64_t>((modified_time.QuadPart - kWindowsToUnixEpoch100ns) / 10000000ULL);

  return FileMetadata{
      .size = static_cast<size_t>(size.QuadPart),
      .modified_seconds = modified_seconds,
      .modified_time =
          std::chrono::system_clock::time_point{std::chrono::seconds{modified_seconds}},
  };
}

// 去掉条件请求头两端的空白，避免匹配时受逗号分段或客户端格式影响。
auto trim_http_header_value(std::string_view value) -> std::string_view {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.remove_suffix(1);
  }
  return value;
}

// 为默认静态资源生成强缓存头；自定义 resolver 可再覆盖成更具体的策略。
auto build_cache_control_header(std::chrono::seconds cache_duration, bool immutable = false)
    -> std::string {
  return std::format("public, max-age={}{}", cache_duration.count(),
                     immutable ? ", immutable" : "");
}

// 基于文件大小和最后修改时间构造条件缓存校验器，避免为原图额外计算内容哈希。
auto build_cache_validators(const FileMetadata& metadata) -> CacheValidators {
  return CacheValidators{
      .etag = std::format("\"{:x}-{:x}\"", metadata.size, metadata.modified_seconds),
      .last_modified = std::format("{:%a, %d %b %Y %H:%M:%S GMT}", metadata.modified_time)};
}

// HTTP 的 If-None-Match 允许逗号分隔多个 ETag；这里只要任一命中即可视为未变更。
auto if_none_match_matches(std::string_view header_value, std::string_view etag) -> bool {
  auto remaining = header_value;
  while (!remaining.empty()) {
    auto comma_pos = remaining.find(',');
    auto candidate = trim_http_header_value(remaining.substr(0, comma_pos));
    if (candidate == "*" || candidate == etag) {
      return true;
    }

    if (comma_pos == std::string_view::npos) {
      break;
    }
    remaining.remove_prefix(comma_pos + 1);
  }
  return false;
}

// 统一判断当前请求是否满足 304 条件；Range 请求保持走实体响应，避免和部分内容语义混淆。
auto is_not_modified_request(auto* req, const CacheValidators& validators, bool has_range_request)
    -> bool {
  if (has_range_request) {
    return false;
  }

  auto if_none_match = trim_http_header_value(std::string_view(req->getHeader("if-none-match")));
  if (!if_none_match.empty()) {
    return if_none_match_matches(if_none_match, validators.etag);
  }

  auto if_modified_since =
      trim_http_header_value(std::string_view(req->getHeader("if-modified-since")));
  if (!if_modified_since.empty()) {
    return if_modified_since == validators.last_modified;
  }

  return false;
}

// 写出文件响应的公共缓存/范围头；200 与 206 响应共用这套头部逻辑。
auto write_common_file_headers(auto* res, const std::string& mime_type,
                               std::string_view cache_control, const CacheValidators& validators,
                               std::optional<size_t> source_file_size = std::nullopt,
                               std::optional<ByteRange> range = std::nullopt,
                               std::optional<std::string_view> content_disposition = std::nullopt,
                               bool allow_range = true) -> void {
  res->writeHeader("Content-Type", get_response_content_type(mime_type));
  if (content_disposition.has_value()) {
    res->writeHeader("Content-Disposition", std::string(*content_disposition));
  }
  res->writeHeader("Cache-Control", std::string(cache_control));
  res->writeHeader("X-Content-Type-Options", "nosniff");
  if (allow_range) {
    res->writeHeader("Accept-Ranges", "bytes");
  }
  res->writeHeader("ETag", validators.etag);
  res->writeHeader("Last-Modified", validators.last_modified);

  if (range.has_value() && source_file_size.has_value()) {
    res->writeHeader("Content-Range", std::format("bytes {}-{}/{}", range->start, range->end,
                                                  source_file_size.value()));
  }
}

// 304 响应不返回实体，但仍需回写缓存校验头，让浏览器更新缓存元数据。
auto write_not_modified(auto* res, std::string_view cache_control,
                        const CacheValidators& validators) -> void {
  res->writeStatus("304 Not Modified");
  res->writeHeader("Cache-Control", std::string(cache_control));
  res->writeHeader("ETag", validators.etag);
  res->writeHeader("Last-Modified", validators.last_modified);
  res->end();
}

auto write_range_not_satisfiable(auto* res, size_t file_size) -> void {
  res->writeStatus("416 Range Not Satisfiable");
  res->writeHeader("Accept-Ranges", "bytes");
  res->writeHeader("Content-Range", std::format("bytes */{}", file_size));
  res->end();
}

// 关闭已完成的流，并执行一次性归档的完成回调。
auto complete_stream(const std::shared_ptr<StreamContext>& ctx) -> void {
  if (ctx->abort_flag->load() || ctx->completion_called) {
    // 中止或已完成的流不再重复关闭文件和触发回调。
    return;
  }

  ctx->completion_called = true;
  // 先释放文件句柄，确保回调可以在 Windows 上删除归档。
  std::error_code close_error;
  ctx->file.close(close_error);
  if (close_error) {
    Logger().warn("Failed to close streamed file '{}': {}", ctx->file_path.string(),
                  close_error.message());
  }
  if (ctx->on_complete) {
    // 完整响应已经发送，通知归档所属功能回收临时文件。
    auto on_complete = std::move(ctx->on_complete);
    on_complete();
  }
  Logger().debug("Stream completed: {}, sent {} bytes", ctx->file_path.string(), ctx->bytes_sent);
}

// 在 uWS 线程中发送数据块
auto send_chunk_to_uws(std::shared_ptr<StreamContext> ctx, std::shared_ptr<std::string> chunk_data)
    -> void {
  if (ctx->abort_flag->load()) {
    Logger().debug("Stream aborted, stopping");
    return;
  }

  // 记录发送前的偏移量（用于处理背压）
  size_t chunk_start_offset = ctx->bytes_sent;

  // tryEnd 的 total 必须为「整个 HTTP 响应体」长度；Range 时为片段长而非文件全长。
  auto [ok, done] = ctx->res->tryEnd(*chunk_data, ctx->response_size);

  if (done) {
    // tryEnd 已经完成响应；不要再访问响应对象或调度下一次读取。
    // tryEnd 已确认整个响应体已交给 uWS，进入完成收尾。
    ctx->bytes_sent = ctx->response_size;
    ctx->file_offset = ctx->file_end_offset;
    complete_stream(ctx);
    return;
  }

  if (!ok) {
    // 背压：缓冲区满，需要等待可写
    ctx->res->onWritable([ctx, chunk_data, chunk_start_offset](size_t) -> bool {
      if (ctx->abort_flag->load()) {
        return false;  // 停止等待
      }

      // 计算已经发送的字节数
      size_t already_sent = ctx->res->getWriteOffset() - chunk_start_offset;

      if (already_sent >= chunk_data->size()) {
        // 这个块已经全部发送完成
        ctx->bytes_sent = ctx->res->getWriteOffset();
        ctx->file_offset += chunk_data->size();

        // onWritable 的返回值表示写入是否成功，不负责移除回调。
        ctx->res->onWritable(nullptr);

        // 继续读下一块
        read_and_send_next_chunk(ctx);
        return true;
      }

      // 发送剩余数据
      auto remaining = chunk_data->substr(already_sent);
      auto [ok2, done2] = ctx->res->tryEnd(remaining, ctx->response_size);

      if (done2) {
        // tryEnd 已经完成响应；它会清除 onWritable。
        // 分段背压恢复后也可能直接完成整个响应，统一走完成回调。
        ctx->bytes_sent = ctx->response_size;
        ctx->file_offset = ctx->file_end_offset;
        complete_stream(ctx);
        return true;
      }

      if (ok2) {
        // 发送成功，显式移除本次背压回调。
        ctx->bytes_sent = ctx->res->getWriteOffset();
        ctx->file_offset += chunk_data->size();
        ctx->res->onWritable(nullptr);

        // 继续读下一块
        read_and_send_next_chunk(ctx);
        return true;
      }

      // 继续等待
      return true;
    });
  } else {
    // 发送成功，更新状态
    ctx->bytes_sent = ctx->res->getWriteOffset();
    ctx->file_offset += chunk_data->size();

    // 继续读下一块
    read_and_send_next_chunk(ctx);
  }
}

// 读取并发送下一个数据块
auto read_and_send_next_chunk(std::shared_ptr<StreamContext> ctx) -> void {
  // 检查是否完成
  if (ctx->file_offset >= ctx->file_end_offset || ctx->abort_flag->load()) {
    if (!ctx->abort_flag->load()) {
      if (ctx->bytes_sent == ctx->response_size) {
        complete_stream(ctx);
      } else {
        Logger().error("Stream ended before the full response was sent: {}",
                       ctx->file_path.string());
      }
    }
    return;
  }

  // 计算本次读取大小
  size_t to_read = std::min(STREAM_CHUNK_SIZE, ctx->file_end_offset - ctx->file_offset);

  // 异步读取文件块
  ctx->file.async_read_some_at(
      ctx->file_offset, asio::buffer(ctx->buffer.data(), to_read),
      [ctx](std::error_code ec, size_t bytes_read) {
        if (ctx->abort_flag->load()) {
          return;
        }

        if (ec || bytes_read == 0) {
          // 文件读取失败时结束响应，但不触发“完整传输”的删除回调。
          Logger().error("Failed to read file {}: {}", ctx->file_path.string(),
                         ec ? ec.message() : "EOF");
          ctx->loop->defer([ctx]() {
            if (ctx->abort_flag->load()) {
              return;
            }
            ctx->res->writeStatus("500 Internal Server Error");
            ctx->res->end("Internal server error");
          });
          return;
        }

        // 准备发送的数据（拷贝到独立的 string）
        auto chunk_data = std::make_shared<std::string>(ctx->buffer.data(), bytes_read);

        // 在 uWS 线程中发送
        ctx->loop->defer([ctx, chunk_data]() {
          if (ctx->abort_flag->load()) {
            return;
          }
          send_chunk_to_uws(ctx, chunk_data);
        });
      });
}

// 按块异步读取大文件，并在完整发送后执行完成回调。
auto handle_file_stream(core::AppState& state, std::filesystem::path file_path,
                        std::string mime_type, std::string cache_control,
                        CacheValidators validators, size_t file_size,
                        std::optional<ByteRange> range,
                        std::optional<std::string> content_disposition, bool allow_range,
                        std::move_only_function<void()> on_complete, auto* res) -> void {
  auto* loop = uWS::Loop::get();
  auto io_context = core::async::get_io_context(state);

  size_t range_start = range.has_value() ? range->start : 0;
  size_t range_end = range.has_value() ? range->end : (file_size - 1);
  size_t response_size = range_end >= range_start ? (range_end - range_start + 1) : 0;
  auto abort_flag = std::make_shared<std::atomic_bool>(false);

  // 必须在异步打开文件前注册；否则客户端可能已经中止而流上下文尚未建立。
  res->onAborted([abort_flag, file_path]() {
    abort_flag->store(true);
    Logger().debug("Stream aborted for: {}", file_path.string());
  });

  // 对于大文件或分片请求，始终按偏移流式发送，避免把整段视频先读进内存。
  // 切到 ASIO 线程打开文件并初始化流上下文。
  asio::post(*io_context, [abort_flag, res, file_path, mime_type,
                           cache_control = std::move(cache_control),
                           validators = std::move(validators),
                           content_disposition = std::move(content_disposition), loop, io_context,
                           file_size, range, range_start, range_end, response_size, allow_range,
                           on_complete = std::move(on_complete)]() mutable {
    try {
      // 在异步线程打开文件，避免阻塞 uWS 事件循环。
      asio::random_access_file file(*io_context, file_path.string(), asio::file_base::read_only);

      Logger().debug("Starting stream for file: {}, size: {} bytes", file_path.string(), file_size);

      // 保存文件、响应和清理回调的共享状态，供后续读写步骤使用。
      auto ctx = std::make_shared<StreamContext>(StreamContext{
          .file = std::move(file),
          .file_path = file_path,
          .source_file_size = file_size,
          .response_size = response_size,
          .file_offset = range_start,
          .file_end_offset = range_end + 1,
          .mime_type = mime_type,
          .cache_control = cache_control,
          .etag = validators.etag,
          .last_modified = validators.last_modified,
          .bytes_sent = 0,
          .status_code = range.has_value() ? 206 : 200,
          .content_range_header = range.has_value()
                                      ? std::optional<std::string>{std::format(
                                            "bytes {}-{}/{}", range_start, range_end, file_size)}
                                      : std::nullopt,
          .content_disposition = std::move(content_disposition),
          .accepts_ranges = allow_range,
          .loop = loop,
          .res = res,
          .buffer = std::vector<char>(STREAM_CHUNK_SIZE),
          .on_complete = std::move(on_complete),
          .completion_called = false,
          .abort_flag = abort_flag,
      });

      // 回到 uWS 线程写响应头，再启动第一轮异步读取。
      loop->defer([ctx]() {
        if (ctx->abort_flag->load()) {
          return;
        }

        ctx->res->writeStatus(ctx->status_code == 206 ? "206 Partial Content" : "200 OK");
        write_common_file_headers(
            ctx->res, ctx->mime_type, ctx->cache_control,
            CacheValidators{.etag = ctx->etag, .last_modified = ctx->last_modified},
            ctx->source_file_size,
            ctx->content_range_header.has_value()
                ? std::optional<ByteRange>{ByteRange{.start = ctx->file_offset,
                                                     .end = ctx->file_end_offset - 1}}
                : std::nullopt,
            ctx->content_disposition.has_value()
                ? std::optional<std::string_view>{*ctx->content_disposition}
                : std::nullopt,
            ctx->accepts_ranges);

        // 开始读取并发送第一块
        read_and_send_next_chunk(ctx);
      });

    } catch (const std::exception& e) {
      // 打开文件失败只能返回服务端错误，不能执行归档完成回调。
      Logger().error("Error opening file for stream {}: {}", file_path.string(), e.what());
      loop->defer([abort_flag, res]() {
        if (abort_flag->load()) {
          return;
        }
        res->writeStatus("500 Internal Server Error");
        res->end("Internal server error");
      });
    }
  });
}

// 图库 /static 原文件与磁盘 web 根路径共用：统一处理 Range、HEAD/GET，并择流式或整读。
auto serve_resolved_file_request(
    core::AppState& state, const std::filesystem::path& file_path, std::string_view url_path,
    std::optional<std::chrono::seconds> cache_duration_override,
    std::optional<std::string> cache_control_override, auto* res, auto* req, bool is_head,
    std::optional<std::string> content_disposition_override = std::nullopt, bool allow_range = true,
    std::move_only_function<void()> on_complete = {}) -> void {
  // 先读取统一元数据，后续 Range、缓存和响应头都使用同一份快照。
  auto metadata_result = query_file_metadata(file_path);
  if (!metadata_result) {
    Logger().warn("Resolved file unavailable: {} ({})", file_path.string(),
                  metadata_result.error());
    res->writeStatus("404 Not Found");
    res->end("File not found");
    return;
  }

  const auto& metadata = metadata_result.value();
  const size_t file_size = metadata.size;

  // 决定mime类型和缓存时间
  std::string mime_type = utils::file::mime::get_mime_type(file_path);
  std::chrono::seconds cache_duration;
  if (cache_duration_override) {
    cache_duration = *cache_duration_override;
  } else {
    cache_duration = get_cache_duration(url_path, file_path);
  }
  auto cache_control =
      cache_control_override
          ? std::move(*cache_control_override)
          : build_cache_control_header(cache_duration, url_path.starts_with("/assets/"));

  auto validators = build_cache_validators(metadata);

  // 一次性归档忽略 Range，确保只有完整响应才会触发删除回调。
  auto range_parse = allow_range ? parse_range_header(req->getHeader("range"), file_size)
                                 : RangeHeaderParseResult{};
  if (!range_parse.valid) {
    write_range_not_satisfiable(res, file_size);
    return;
  }

  // 临时归档必须真正发送完整内容，不走 304 短路。
  if (allow_range && is_not_modified_request(req, validators, range_parse.range.has_value())) {
    write_not_modified(res, cache_control, validators);
    return;
  }

  size_t content_length = range_parse.range.has_value()
                              ? (range_parse.range->end - range_parse.range->start + 1)
                              : file_size;

  if (is_head) {
    // HEAD 只返回元数据，不触发一次性归档的删除回调。
    res->writeStatus(range_parse.range.has_value() ? "206 Partial Content" : "200 OK");
    write_common_file_headers(res, mime_type, cache_control, validators, file_size,
                              range_parse.range,
                              content_disposition_override.has_value()
                                  ? std::optional<std::string_view>{*content_disposition_override}
                                  : std::nullopt,
                              allow_range);
    res->writeHeader("Content-Length", std::to_string(content_length));
    res->end();
    return;
  }

  // 视频任意 Range 都应流式发送，避免小 Range 却整文件读入内存（content_length 可能很小但 file
  // 很大）。
  if (content_length > STREAM_THRESHOLD || file_size > STREAM_THRESHOLD) {
    Logger().debug("Using stream for resolved file: {} bytes", file_size);
    handle_file_stream(state, file_path, mime_type, cache_control, validators, file_size,
                       range_parse.range, std::move(content_disposition_override), allow_range,
                       std::move(on_complete), res);
    return;
  }

  Logger().debug("Using single-read for small resolved file: {} bytes", file_size);

  // 小文件整读仍放到异步运行时，避免阻塞 HTTP 事件循环。
  auto* loop = uWS::Loop::get();
  auto* io_context = core::async::get_io_context(state);
  if (!io_context) {
    res->writeStatus("500 Internal Server Error");
    res->end("Internal server error");
    return;
  }

  auto abort_flag = std::make_shared<std::atomic_bool>(false);
  res->onAborted([abort_flag, file_path]() {
    abort_flag->store(true);
    Logger().debug("Single-read aborted for: {}", file_path.string());
  });
  // 让异步协程和 uWS 回调共享一次性的完成处理器。
  auto completion = std::make_shared<std::move_only_function<void()>>(std::move(on_complete));

  // 在异步运行时中读取文件，再回到 uWS 线程发送响应。
  asio::co_spawn(
      *io_context,
      [res, file_path, mime_type, cache_control = std::move(cache_control),
       validators = std::move(validators),
       content_disposition = std::move(content_disposition_override), loop, file_size,
       range = range_parse.range, allow_range, abort_flag, completion]() -> asio::awaitable<void> {
        try {
          // 读取整文件后再按请求的 Range 截取响应体。
          auto file_result = co_await utils::file::read_file(file_path, file_size);
          if (!file_result) {
            Logger().error("Failed to read custom file: {}", file_result.error());
            loop->defer([res, abort_flag]() {
              if (abort_flag->load()) {
                return;
              }
              res->writeStatus("500 Internal Server Error");
              res->end("Internal server error");
            });
            co_return;
          }

          auto file_data = std::move(file_result.value());
          if (file_data.data.size() < file_size) {
            Logger().error("File changed while reading: {}", file_path.string());
            loop->defer([res, abort_flag]() {
              if (abort_flag->load()) {
                return;
              }
              res->writeStatus("500 Internal Server Error");
              res->end("Internal server error");
            });
            co_return;
          }

          const size_t range_start = range.has_value() ? range->start : 0;
          const size_t range_end =
              file_size > 0 ? (range.has_value() ? range->end : file_size - 1) : 0;
          const size_t content_length =
              file_size > 0 && range_end >= range_start ? (range_end - range_start + 1) : 0;

          std::string response_body;
          if (content_length > 0) {
            response_body.assign(reinterpret_cast<const char*>(file_data.data.data() + range_start),
                                 content_length);
          }

          // 只有响应成功结束后才执行一次性归档的完成回调。
          loop->defer([res, file_path, mime_type, cache_control, validators, file_size, range,
                       content_disposition = std::move(content_disposition),
                       response_body = std::move(response_body), allow_range, abort_flag,
                       completion]() mutable {
            if (abort_flag->load()) {
              return;
            }
            res->writeStatus(range.has_value() ? "206 Partial Content" : "200 OK");
            write_common_file_headers(res, mime_type, cache_control, validators, file_size, range,
                                      content_disposition.has_value()
                                          ? std::optional<std::string_view>{*content_disposition}
                                          : std::nullopt,
                                      allow_range);
            res->end(response_body);

            if (*completion) {
              // 移出回调再执行，避免同一响应重复触发删除。
              auto on_complete = std::move(*completion);
              on_complete();
            }

            Logger().debug("Served resolved file: {}, size: {} bytes", file_path.string(),
                           response_body.size());
          });

        } catch (const std::exception& e) {
          // 读取或组装响应失败时不认为文件已完成传输。
          Logger().error("Error serving resolved file {}: {}", file_path.string(), e.what());
          loop->defer([res, abort_flag]() {
            if (abort_flag->load()) {
              return;
            }
            res->writeStatus("500 Internal Server Error");
            res->end("Internal server error");
          });
        }
      },
      core::async::log_completion("Static file request"));
}

// 构造兼容 ASCII 文件名和 UTF-8 filename* 的附件响应头。
auto build_download_content_disposition(std::string_view download_name) -> std::string {
  std::string fallback_name;
  fallback_name.reserve(download_name.size());
  for (const auto character : download_name) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte >= 0x20 && byte <= 0x7E && character != '"' && character != '\\' && character != '/') {
      fallback_name.push_back(character);
    } else {
      fallback_name.push_back('_');
    }
  }
  if (fallback_name.empty()) {
    fallback_name = "download";
  }

  constexpr char hex_digits[] = "0123456789ABCDEF";
  std::string encoded_name;
  encoded_name.reserve(download_name.size() * 3);
  for (const auto character : download_name) {
    const auto byte = static_cast<unsigned char>(character);
    const bool unreserved = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
                            (byte >= '0' && byte <= '9') || byte == '-' || byte == '.' ||
                            byte == '_' || byte == '~';
    if (unreserved) {
      encoded_name.push_back(static_cast<char>(byte));
    } else {
      encoded_name.push_back('%');
      encoded_name.push_back(hex_digits[(byte >> 4) & 0x0F]);
      encoded_name.push_back(hex_digits[byte & 0x0F]);
    }
  }

  return std::format("attachment; filename=\"{}\"; filename*=UTF-8''{}", fallback_name,
                     encoded_name);
}

// 按下载策略发送附件文件，并把完成回调传给具体传输分支。
auto serve_download_file_request(core::AppState& state, const std::filesystem::path& file_path,
                                 std::string download_name, uWS::HttpResponse<false>* res,
                                 uWS::HttpRequest* req, bool allow_range,
                                 std::move_only_function<void()> on_complete) -> void {
  serve_resolved_file_request(state, file_path, "/downloads/", std::chrono::seconds{0},
                              std::string{"no-store"}, res, req, false,
                              build_download_content_disposition(download_name), allow_range,
                              std::move(on_complete));
}

// 处理静态文件请求
auto handle_static_request(core::AppState& state, const std::string& url_path, auto* res, auto* req,
                           bool is_head = false) -> void {
  // 静态页面和资源也属于远端会话的一部分，未认证请求不能读取应用内容。
  if (!core::http_server::access::resolve_http_access(state, res->getRemoteAddressAsText(),
                                                      req->getHeader("cookie"))) {
    // 先写 401，避免鉴权失败响应被默认成 200。
    res->writeStatus("401 Unauthorized");
    res->writeHeader("Cache-Control", "no-store");
    res->writeHeader("Content-Type", "text/plain; charset=utf-8");
    res->end("Authentication required");
    return;
  }

  // 1. 先尝试自定义解析器
  if (auto custom_result = try_custom_resolve(state, url_path)) {
    if (custom_result->has_value()) {
      Logger().debug("Using custom resolver for: {}", url_path);
      serve_resolved_file_request(state, custom_result->value().file_path, url_path,
                                  custom_result->value().cache_duration,
                                  custom_result->value().cache_control_header, res, req, is_head);
      return;
    }
  }

  // 2. 否则使用默认的 web 资源解析
  auto web_root = get_web_root();
  auto file_path = resolve_file_path(web_root, url_path);

  // 路径安全检查
  if (!utils::path::IsPathWithinBase(file_path, web_root)) {
    Logger().warn("Unsafe path requested: {}", file_path.string());
    res->writeStatus("403 Forbidden");
    res->end("Forbidden");
    return;
  }

  serve_resolved_file_request(state, file_path, url_path, std::nullopt, std::nullopt, res, req,
                              is_head);
}

// 注册静态文件路由
auto register_routes(core::AppState& state, uWS::App& app) -> void {
  Logger().info("Registering static file routes");

  // 注册通用的GET路由处理所有静态文件请求
  app.get("/*", [&state](auto* res, auto* req) {
    std::string url = std::string(req->getUrl());
    Logger().debug("Static file request: {}", url);

    // 先注册通用中止日志；流式处理会在 cork 内用自己的 abort_flag 覆盖它。
    res->onAborted([]() { Logger().debug("Static file request aborted"); });

    // cork 只批量提交同步写入，不延长异步操作中 res 的生命周期。
    res->cork([&state, res, req, url]() {
      Logger().debug("Corking static file request: {}", url);
      // 处理静态文件请求
      handle_static_request(state, url, res, req, false);
    });
  });

  // 也处理HEAD请求（用于文件存在性检查）
  app.head("/*", [&state](auto* res, auto* req) {
    std::string url = std::string(req->getUrl());
    Logger().debug("Static file HEAD request: {}", url);
    handle_static_request(state, url, res, req, true);
  });
}

}  // namespace core::http_server::static_content
