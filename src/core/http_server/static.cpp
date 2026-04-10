module;

#include <uwebsockets/App.h>

#include <asio.hpp>

module Core.HttpServer.Static;

import std;
import Core.State;
import Core.HttpServer.Types;
import Core.Async;
import Utils.File;
import Utils.File.Mime;
import Utils.Path;
import Utils.Logger;
import Utils.Time;

namespace Core::HttpServer::Static {

auto register_path_resolver(Core::State::AppState& state, std::string prefix,
                            Types::PathResolver resolver) -> void {
  if (!state.http_server) {
    Logger().error("HttpServer state not initialized, cannot register path resolver");
    return;
  }

  auto& registry = state.http_server->path_resolvers;
  std::unique_lock lock(registry.write_mutex);

  auto current = registry.resolvers.load();
  auto new_resolvers = std::make_shared<std::vector<Types::ResolverEntry>>(*current);
  new_resolvers->push_back({std::move(prefix), std::move(resolver)});
  registry.resolvers.store(new_resolvers);

  Logger().debug("Registered custom path resolver for: {}", prefix);
}

auto unregister_path_resolver(Core::State::AppState& state, std::string_view prefix) -> void {
  if (!state.http_server) {
    return;
  }

  auto& registry = state.http_server->path_resolvers;
  std::unique_lock lock(registry.write_mutex);

  auto current = registry.resolvers.load();
  auto new_resolvers = std::make_shared<std::vector<Types::ResolverEntry>>(*current);
  std::erase_if(*new_resolvers, [prefix](const auto& entry) { return entry.prefix == prefix; });
  registry.resolvers.store(new_resolvers);

  Logger().debug("Unregistered path resolver for: {}", prefix);
}

auto try_custom_resolve(Core::State::AppState& state, std::string_view url_path)
    -> std::optional<Types::PathResolution> {
  if (!state.http_server) {
    return std::nullopt;
  }

  auto& registry = state.http_server->path_resolvers;
  auto resolvers = registry.resolvers.load();

  for (const auto& entry : *resolvers) {
    if (url_path.starts_with(entry.prefix)) {
      auto result = entry.resolver(url_path);
      if (result.has_value()) {
        return result;
      }
    }
  }
  return std::nullopt;
}

auto is_safe_path(const std::filesystem::path& path, const std::filesystem::path& base_path)
    -> std::expected<bool, std::string> {
  try {
    std::filesystem::path normalized_path = path.lexically_normal();
    std::filesystem::path normalized_base = base_path.lexically_normal();

    std::string path_str = normalized_path.string();
    std::string base_str = normalized_base.string();

    // 检查路径是否以基础路径开头，或者路径等于基础路径
    bool is_safe = path_str.rfind(base_str, 0) == 0 || normalized_path == normalized_base;
    return is_safe;
  } catch (const std::exception& e) {
    return std::unexpected("Failed to check path safety: " + std::string(e.what()));
  }
}

// 获取针对不同文件类型的缓存时间
auto get_cache_duration(const std::string& extension) -> std::chrono::seconds {
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
auto resolve_file_path(const std::string& url_path) -> std::filesystem::path {
  auto web_root = Utils::Path::GetEmbeddedWebRootDirectory().value_or(".");

  auto clean_path = url_path == "/" ? "/index.html" : url_path;
  if (clean_path.ends_with("/")) clean_path += "index.html";

  return web_root / clean_path.substr(1);  // 移除开头的'/'
}

// 获取web根目录
auto get_web_root() -> std::filesystem::path {
  return Utils::Path::GetEmbeddedWebRootDirectory().value_or(".");
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
auto build_cache_control_header(std::chrono::seconds cache_duration) -> std::string {
  return std::format("public, max-age={}", cache_duration.count());
}

// 基于文件大小和最后修改时间构造条件缓存校验器，避免为原图额外计算内容哈希。
auto build_cache_validators(const std::filesystem::path& file_path, size_t file_size)
    -> std::expected<CacheValidators, std::string> {
  std::error_code ec;
  auto last_write_time = std::filesystem::last_write_time(file_path, ec);
  if (ec) {
    return std::unexpected("Failed to query file last write time: " + ec.message());
  }

  auto modified_time = std::chrono::time_point_cast<std::chrono::seconds>(
      Utils::Time::file_time_to_system_clock(last_write_time));
  auto modified_seconds = Utils::Time::file_time_to_seconds(last_write_time);

  return CacheValidators{
      .etag = std::format("\"{:x}-{:x}\"", file_size, modified_seconds),
      .last_modified = std::format("{:%a, %d %b %Y %H:%M:%S GMT}", modified_time)};
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
                               std::optional<ByteRange> range = std::nullopt) -> void {
  res->writeHeader("Content-Type", get_response_content_type(mime_type));
  res->writeHeader("Cache-Control", std::string(cache_control));
  res->writeHeader("X-Content-Type-Options", "nosniff");
  res->writeHeader("Accept-Ranges", "bytes");
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

// 在 uWS 线程中发送数据块
auto send_chunk_to_uws(std::shared_ptr<Types::StreamContext> ctx,
                       std::shared_ptr<std::string> chunk_data) -> void {
  if (ctx->is_aborted) {
    Logger().debug("Stream aborted, stopping");
    return;
  }

  // 记录发送前的偏移量（用于处理背压）
  size_t chunk_start_offset = ctx->bytes_sent;

  // tryEnd 的 total 必须为「整个 HTTP 响应体」长度；Range 时为片段长而非文件全长。
  auto [ok, done] = ctx->res->tryEnd(*chunk_data, ctx->response_size);

  if (!ok) {
    // 背压：缓冲区满，需要等待可写
    Logger().debug("Backpressure detected, waiting for writable");

    ctx->res->onWritable([ctx, chunk_data, chunk_start_offset](size_t) -> bool {
      if (ctx->is_aborted) {
        return false;  // 停止等待
      }

      // 计算已经发送的字节数
      size_t already_sent = ctx->res->getWriteOffset() - chunk_start_offset;

      if (already_sent >= chunk_data->size()) {
        // 这个块已经全部发送完成
        ctx->bytes_sent = ctx->res->getWriteOffset();
        ctx->file_offset += chunk_data->size();

        // 继续读下一块
        read_and_send_next_chunk(ctx);
        return false;  // 移除 onWritable
      }

      // 发送剩余数据
      auto remaining = chunk_data->substr(already_sent);
      auto [ok2, done2] = ctx->res->tryEnd(remaining, ctx->response_size);

      if (ok2 || done2) {
        // 发送成功
        ctx->bytes_sent = ctx->res->getWriteOffset();
        ctx->file_offset += chunk_data->size();

        // 继续读下一块
        read_and_send_next_chunk(ctx);
        return false;  // 移除 onWritable
      }

      // 继续等待
      return true;
    });
  } else {
    // 发送成功，更新状态
    ctx->bytes_sent = ctx->res->getWriteOffset();
    ctx->file_offset += chunk_data->size();

    if (done) {
      // 整个响应已完成
      Logger().debug("Stream completed: {}, sent {} bytes", ctx->file_path.string(),
                     ctx->bytes_sent);
    } else {
      // 继续读下一块
      read_and_send_next_chunk(ctx);
    }
  }
}

// 读取并发送下一个数据块
auto read_and_send_next_chunk(std::shared_ptr<Types::StreamContext> ctx) -> void {
  // 检查是否完成
  if (ctx->file_offset >= ctx->file_end_offset || ctx->is_aborted) {
    if (!ctx->is_aborted) {
      Logger().debug("Stream completed: {}, sent {} bytes", ctx->file_path.string(),
                     ctx->bytes_sent);
    }
    return;
  }

  // 计算本次读取大小
  size_t to_read = std::min(Types::STREAM_CHUNK_SIZE, ctx->file_end_offset - ctx->file_offset);

  // 异步读取文件块
  ctx->file.async_read_some_at(
      ctx->file_offset, asio::buffer(ctx->buffer.data(), to_read),
      [ctx](std::error_code ec, size_t bytes_read) {
        if (ec || bytes_read == 0) {
          Logger().error("Failed to read file {}: {}", ctx->file_path.string(),
                         ec ? ec.message() : "EOF");
          ctx->loop->defer([ctx]() {
            ctx->res->writeStatus("500 Internal Server Error");
            ctx->res->end("Internal server error");
          });
          return;
        }

        // 准备发送的数据（拷贝到独立的 string）
        auto chunk_data = std::make_shared<std::string>(ctx->buffer.data(), bytes_read);

        // 在 uWS 线程中发送
        ctx->loop->defer([ctx, chunk_data]() { send_chunk_to_uws(ctx, chunk_data); });
      });
}

// 流式传输文件
auto handle_file_stream(Core::State::AppState& state, std::filesystem::path file_path,
                        std::string mime_type, std::string cache_control,
                        CacheValidators validators, size_t file_size,
                        std::optional<ByteRange> range, auto* res) -> void {
  auto* loop = uWS::Loop::get();
  auto io_context = Core::Async::get_io_context(*state.async);

  size_t range_start = range.has_value() ? range->start : 0;
  size_t range_end = range.has_value() ? range->end : (file_size - 1);
  size_t response_size = range_end >= range_start ? (range_end - range_start + 1) : 0;

  // 对于大文件或分片请求，始终按偏移流式发送，避免把整段视频先读进内存。
  // 在 ASIO 线程中打开文件并初始化
  asio::post(*io_context, [res, file_path, mime_type, cache_control = std::move(cache_control),
                           validators = std::move(validators), loop, io_context, file_size, range,
                           range_start, range_end, response_size]() {
    try {
      // 打开文件
      asio::random_access_file file(*io_context, file_path.string(), asio::file_base::read_only);

      Logger().debug("Starting stream for file: {}, size: {} bytes", file_path.string(), file_size);

      // 创建流上下文
      auto ctx = std::make_shared<Types::StreamContext>(Types::StreamContext{
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
          .loop = loop,
          .res = res,
          .buffer = std::vector<char>(Types::STREAM_CHUNK_SIZE),
          .is_aborted = false,
      });

      // 在 uWS 线程中设置响应头并开始传输
      loop->defer([ctx]() {
        ctx->res->writeStatus(ctx->status_code == 206 ? "206 Partial Content" : "200 OK");
        write_common_file_headers(
            ctx->res, ctx->mime_type, ctx->cache_control,
            CacheValidators{.etag = ctx->etag, .last_modified = ctx->last_modified},
            ctx->source_file_size,
            ctx->content_range_header.has_value()
                ? std::optional<ByteRange>{ByteRange{.start = ctx->file_offset,
                                                     .end = ctx->file_end_offset - 1}}
                : std::nullopt);

        // 处理中止
        ctx->res->onAborted([ctx]() {
          Logger().debug("Stream aborted for: {}", ctx->file_path.string());
          ctx->is_aborted = true;
        });

        // 开始读取并发送第一块
        read_and_send_next_chunk(ctx);
      });

    } catch (const std::exception& e) {
      Logger().error("Error opening file for stream {}: {}", file_path.string(), e.what());
      loop->defer([res]() {
        res->writeStatus("500 Internal Server Error");
        res->end("Internal server error");
      });
    }
  });
}

// 图库 /static 原文件与磁盘 web 根路径共用：统一处理 Range、HEAD/GET，并择流式或整读。
auto serve_resolved_file_request(Core::State::AppState& state,
                                 const std::filesystem::path& file_path,
                                 std::optional<std::chrono::seconds> cache_duration_override,
                                 std::optional<std::string> cache_control_override, auto* res,
                                 auto* req, bool is_head) -> void {
  // 检查文件是否存在
  if (!std::filesystem::exists(file_path)) {
    Logger().warn("Resolved file not found: {}", file_path.string());
    res->writeStatus("404 Not Found");
    res->end("File not found");
    return;
  }

  // 获取文件大小
  size_t file_size = std::filesystem::file_size(file_path);

  // 决定mime类型和缓存时间
  std::string mime_type = Utils::File::Mime::get_mime_type(file_path);
  std::chrono::seconds cache_duration;
  if (cache_duration_override) {
    cache_duration = *cache_duration_override;
  } else {
    auto extension = file_path.extension().string();
    cache_duration = get_cache_duration(extension);
  }
  auto cache_control = cache_control_override.value_or(build_cache_control_header(cache_duration));

  auto validators_result = build_cache_validators(file_path, file_size);
  if (!validators_result) {
    Logger().error("Failed to build cache validators for {}: {}", file_path.string(),
                   validators_result.error());
    res->writeStatus("500 Internal Server Error");
    res->end("Internal server error");
    return;
  }
  auto validators = std::move(validators_result.value());

  auto range_parse = parse_range_header(std::string(req->getHeader("range")), file_size);
  if (!range_parse.valid) {
    write_range_not_satisfiable(res, file_size);
    return;
  }

  if (is_not_modified_request(req, validators, range_parse.range.has_value())) {
    write_not_modified(res, cache_control, validators);
    return;
  }

  size_t content_length = range_parse.range.has_value()
                              ? (range_parse.range->end - range_parse.range->start + 1)
                              : file_size;

  if (is_head) {
    res->writeStatus(range_parse.range.has_value() ? "206 Partial Content" : "200 OK");
    write_common_file_headers(res, mime_type, cache_control, validators, file_size,
                              range_parse.range);
    res->writeHeader("Content-Length", std::to_string(content_length));
    res->end();
    return;
  }

  // 视频任意 Range 都应流式发送，避免小 Range 却整文件读入内存（content_length 可能很小但 file
  // 很大）。
  if (content_length > Types::STREAM_THRESHOLD || file_size > Types::STREAM_THRESHOLD) {
    Logger().debug("Using stream for resolved file: {} bytes", file_size);
    handle_file_stream(state, file_path, mime_type, cache_control, validators, file_size,
                       range_parse.range, res);
    return;
  }

  Logger().debug("Using single-read for small resolved file: {} bytes", file_size);

  // 获取当前的事件循环
  auto* loop = uWS::Loop::get();

  // 在异步运行时中处理文件读取
  asio::co_spawn(
      *Core::Async::get_io_context(*state.async),
      [res, file_path, mime_type, cache_control = std::move(cache_control),
       validators = std::move(validators), loop, file_size,
       range = range_parse.range]() -> asio::awaitable<void> {
        try {
          // 异步读取文件
          auto file_result = co_await Utils::File::read_file(file_path);
          if (!file_result) {
            Logger().error("Failed to read custom file: {}", file_result.error());
            loop->defer([res]() {
              res->writeStatus("500 Internal Server Error");
              res->end("Internal server error");
            });
            co_return;
          }

          auto file_data = file_result.value();
          size_t range_start = range.has_value() ? range->start : 0;
          size_t range_end = range.has_value() ? range->end : (file_size - 1);
          size_t content_length = range_end >= range_start ? (range_end - range_start + 1) : 0;

          std::string response_body(
              reinterpret_cast<const char*>(file_data.data.data() + range_start), content_length);

          // 在事件循环线程中发送响应
          loop->defer([res, file_path, mime_type, cache_control, validators, file_size, range,
                       response_body = std::move(response_body)]() mutable {
            res->writeStatus(range.has_value() ? "206 Partial Content" : "200 OK");
            write_common_file_headers(res, mime_type, cache_control, validators, file_size, range);
            res->end(response_body);

            Logger().debug("Served resolved file: {}, size: {} bytes", file_path.string(),
                           response_body.size());
          });

        } catch (const std::exception& e) {
          Logger().error("Error serving resolved file {}: {}", file_path.string(), e.what());
          loop->defer([res]() {
            res->writeStatus("500 Internal Server Error");
            res->end("Internal server error");
          });
        }
      },
      asio::detached);
}

// 处理静态文件请求
auto handle_static_request(Core::State::AppState& state, const std::string& url_path, auto* res,
                           auto* req, bool is_head = false) -> void {
  // 1. 先尝试自定义解析器
  if (auto custom_result = try_custom_resolve(state, url_path)) {
    if (custom_result->has_value()) {
      Logger().debug("Using custom resolver for: {}", url_path);
      serve_resolved_file_request(state, custom_result->value().file_path,
                                  custom_result->value().cache_duration,
                                  custom_result->value().cache_control_header, res, req, is_head);
      return;
    }
  }

  // 2. 否则使用默认的 web 资源解析
  auto file_path = resolve_file_path(url_path);
  auto web_root = get_web_root();

  // 路径安全检查
  auto safety_check = is_safe_path(file_path, web_root);
  if (!safety_check.has_value() || !safety_check.value()) {
    Logger().warn("Unsafe path requested: {}", file_path.string());
    res->writeStatus("403 Forbidden");
    res->end("Forbidden");
    return;
  }

  // 检查文件是否存在
  if (!std::filesystem::exists(file_path)) {
    Logger().warn("File not found: {}", file_path.string());
    res->writeStatus("404 Not Found");
    res->end("File not found");
    return;
  }

  serve_resolved_file_request(state, file_path, std::nullopt, std::nullopt, res, req, is_head);
}

// 注册静态文件路由
auto register_routes(Core::State::AppState& state, uWS::App& app) -> void {
  Logger().info("Registering static file routes");

  // 注册通用的GET路由处理所有静态文件请求
  app.get("/*", [&state](auto* res, auto* req) {
    std::string url = std::string(req->getUrl());
    Logger().debug("Static file request: {}", url);

    // 使用 cork 包裹整个异步操作，延长 res 的生命周期
    res->cork([&state, res, req, url]() {
      Logger().debug("Corking static file request: {}", url);
      // 处理静态文件请求
      handle_static_request(state, url, res, req, false);
    });

    // 连接中止时记录日志
    res->onAborted([]() { Logger().debug("Static file request aborted"); });
  });

  // 也处理HEAD请求（用于文件存在性检查）
  app.head("/*", [&state](auto* res, auto* req) {
    std::string url = std::string(req->getUrl());
    Logger().debug("Static file HEAD request: {}", url);
    handle_static_request(state, url, res, req, true);
  });
}

}  // namespace Core::HttpServer::Static
