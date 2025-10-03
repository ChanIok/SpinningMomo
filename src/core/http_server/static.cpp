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
  auto exe_dir = Utils::Path::GetExecutableDirectory().value_or(".");
  auto web_root = exe_dir / "resources" / "web";

  auto clean_path = url_path == "/" ? "/index.html" : url_path;
  if (clean_path.ends_with("/")) clean_path += "index.html";

  return web_root / clean_path.substr(1);  // 移除开头的'/'
}

// 获取web根目录
auto get_web_root() -> std::filesystem::path {
  auto exe_dir = Utils::Path::GetExecutableDirectory().value_or(".");
  return exe_dir / "resources" / "web";
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

  auto [ok, done] = ctx->res->tryEnd(*chunk_data, ctx->file_size);

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
        ctx->offset += chunk_data->size();

        // 继续读下一块
        read_and_send_next_chunk(ctx);
        return false;  // 移除 onWritable
      }

      // 发送剩余数据
      auto remaining = chunk_data->substr(already_sent);
      auto [ok2, done2] = ctx->res->tryEnd(remaining, ctx->file_size);

      if (ok2 || done2) {
        // 发送成功
        ctx->bytes_sent = ctx->res->getWriteOffset();
        ctx->offset += chunk_data->size();

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
    ctx->offset += chunk_data->size();

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
  if (ctx->offset >= ctx->file_size || ctx->is_aborted) {
    if (!ctx->is_aborted) {
      Logger().debug("Stream completed: {}, sent {} bytes", ctx->file_path.string(),
                     ctx->bytes_sent);
    }
    return;
  }

  // 计算本次读取大小
  size_t to_read = std::min(Types::STREAM_CHUNK_SIZE, ctx->file_size - ctx->offset);

  // 异步读取文件块
  ctx->file.async_read_some_at(
      ctx->offset, asio::buffer(ctx->buffer.data(), to_read),
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
                        std::string mime_type, std::chrono::seconds cache_duration, auto* res)
    -> void {
  auto* loop = uWS::Loop::get();
  auto io_context = Core::Async::get_io_context(*state.async);

  // 在 ASIO 线程中打开文件并初始化
  asio::post(*io_context, [res, file_path, mime_type, cache_duration, loop, io_context]() {
    try {
      // 打开文件
      asio::random_access_file file(*io_context, file_path.string(), asio::file_base::read_only);
      auto file_size = file.size();

      Logger().debug("Starting stream for file: {}, size: {} bytes", file_path.string(), file_size);

      // 创建流上下文
      auto ctx = std::make_shared<Types::StreamContext>(Types::StreamContext{
          .file = std::move(file),
          .file_path = file_path,
          .file_size = file_size,
          .offset = 0,
          .mime_type = mime_type,
          .cache_duration = cache_duration,
          .bytes_sent = 0,
          .loop = loop,
          .res = res,
          .buffer = std::vector<char>(Types::STREAM_CHUNK_SIZE),
          .is_aborted = false,
      });

      // 在 uWS 线程中设置响应头并开始传输
      loop->defer([ctx]() {
        ctx->res->writeStatus("200 OK");
        ctx->res->writeHeader("Content-Type", ctx->mime_type);
        ctx->res->writeHeader("Cache-Control",
                              std::format("public, max-age={}", ctx->cache_duration.count()));
        ctx->res->writeHeader("X-Content-Type-Options", "nosniff");

        if (ctx->mime_type.starts_with("text/") && !ctx->mime_type.contains("charset=")) {
          ctx->res->writeHeader("Content-Type", ctx->mime_type + "; charset=utf-8");
        }

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

// 处理自定义解析的文件请求
auto handle_custom_file_request(Core::State::AppState& state,
                                const Types::PathResolutionData& resolution, auto* res) -> void {
  const auto& file_path = resolution.file_path;

  Logger().debug("Serving custom file: {}", file_path.string());

  // 检查文件是否存在
  if (!std::filesystem::exists(file_path)) {
    Logger().warn("Custom file not found: {}", file_path.string());
    res->writeStatus("404 Not Found");
    res->end("File not found");
    return;
  }

  // 获取文件大小
  size_t file_size = std::filesystem::file_size(file_path);

  // 决定mime类型和缓存时间
  std::string mime_type = Utils::File::Mime::get_mime_type(file_path);
  std::chrono::seconds cache_duration;
  if (resolution.cache_duration) {
    cache_duration = *resolution.cache_duration;
  } else {
    auto extension = file_path.extension().string();
    cache_duration = get_cache_duration(extension);
  }

  // 根据文件大小选择处理方式
  if (file_size > Types::STREAM_THRESHOLD) {
    Logger().debug("Using stream for large custom file: {} bytes", file_size);
    handle_file_stream(state, file_path, mime_type, cache_duration, res);
    return;
  }

  // 小文件使用一次性读取
  Logger().debug("Using single-read for small custom file: {} bytes", file_size);

  // 获取当前的事件循环
  auto* loop = uWS::Loop::get();

  // 在异步运行时中处理文件读取
  asio::co_spawn(
      *Core::Async::get_io_context(*state.async),
      [res, file_path, mime_type, cache_duration, loop]() -> asio::awaitable<void> {
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

          // 在事件循环线程中发送响应
          loop->defer([res, file_path, file_data, mime_type, cache_duration]() mutable {
            res->writeHeader("Content-Type", mime_type);
            res->writeHeader("Cache-Control",
                             std::format("public, max-age={}", cache_duration.count()));

            // 设置其他安全相关的头
            res->writeHeader("X-Content-Type-Options", "nosniff");

            // 对于文本文件，设置字符编码
            if (mime_type.starts_with("text/") && !mime_type.contains("charset=")) {
              res->writeHeader("Content-Type", mime_type + "; charset=utf-8");
            }

            res->writeStatus("200 OK");
            res->end(std::string(file_data.data.begin(), file_data.data.end()));

            Logger().debug("Served custom file: {}, size: {} bytes", file_data.path,
                           file_data.original_size);
          });

        } catch (const std::exception& e) {
          Logger().error("Error serving custom file {}: {}", file_path.string(), e.what());
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
                           auto* req) -> void {
  // 1. 先尝试自定义解析器
  if (auto custom_result = try_custom_resolve(state, url_path)) {
    if (custom_result->has_value()) {
      Logger().debug("Using custom resolver for: {}", url_path);
      handle_custom_file_request(state, custom_result->value(), res);
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

  // 获取文件大小
  size_t file_size = std::filesystem::file_size(file_path);

  // 决定MIME类型和缓存时间
  std::string mime_type = Utils::File::Mime::get_mime_type(file_path);
  auto extension = file_path.extension().string();
  auto cache_duration = get_cache_duration(extension);

  // 根据文件大小选择处理方式
  if (file_size > Types::STREAM_THRESHOLD) {
    Logger().debug("Using stream for large file: {} bytes", file_size);
    handle_file_stream(state, file_path, mime_type, cache_duration, res);
    return;
  }

  Logger().debug("Using single-read for small file: {} bytes", file_size);

  // 获取当前的事件循环
  auto* loop = uWS::Loop::get();

  // 在异步运行时中处理文件读取
  asio::co_spawn(
      *Core::Async::get_io_context(*state.async),
      [res, file_path, mime_type, cache_duration, loop]() -> asio::awaitable<void> {
        try {
          // 异步读取文件
          auto file_result = co_await Utils::File::read_file(file_path);
          if (!file_result) {
            Logger().error("Failed to read file: {}", file_result.error());
            loop->defer([res]() {
              res->writeStatus("500 Internal Server Error");
              res->end("Internal server error");
            });
            co_return;
          }

          auto file_data = file_result.value();

          // 在事件循环线程中发送响应
          loop->defer([res, file_data, mime_type, cache_duration]() mutable {
            // 设置Content-Type
            res->writeHeader("Content-Type", mime_type);
            res->writeHeader("Cache-Control",
                             std::format("public, max-age={}", cache_duration.count()));

            // 设置其他安全相关的头
            res->writeHeader("X-Content-Type-Options", "nosniff");

            // 对于文本文件，设置字符编码
            if (mime_type.starts_with("text/") && !mime_type.contains("charset=")) {
              res->writeHeader("Content-Type", mime_type + "; charset=utf-8");
            }

            res->writeStatus("200 OK");
            res->end(std::string(file_data.data.begin(), file_data.data.end()));

            Logger().debug("Served file: {}, size: {} bytes", file_data.path,
                           file_data.original_size);
          });

        } catch (const std::exception& e) {
          Logger().error("Error serving static file {}: {}", file_path.string(), e.what());
          loop->defer([res]() {
            res->writeStatus("500 Internal Server Error");
            res->end("Internal server error");
          });
        }
      },
      asio::detached);
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
      handle_static_request(state, url, res, req);
    });

    // 连接中止时记录日志
    res->onAborted([]() { Logger().debug("Static file request aborted"); });
  });

  // 也处理HEAD请求（用于文件存在性检查）
  app.head("/*", [&state](auto* res, auto* req) {
    std::string url = std::string(req->getUrl());
    Logger().debug("Static file HEAD request: {}", url);

    auto file_path = resolve_file_path(url);
    auto web_root = get_web_root();

    // 路径安全检查
    auto safety_check = is_safe_path(file_path, web_root);
    if (!safety_check.has_value() || !safety_check.value()) {
      res->writeStatus("403 Forbidden");
      res->end();
      return;
    }

    // 检查文件是否存在
    if (!std::filesystem::exists(file_path)) {
      res->writeStatus("404 Not Found");
      res->end();
      return;
    }

    // 设置响应头但不发送内容
    auto extension = file_path.extension().string();
    auto cache_duration = get_cache_duration(extension);
    auto cache_header = std::format("public, max-age={}", cache_duration.count());

    res->writeHeader("Content-Type", Utils::File::Mime::get_mime_type(file_path));
    res->writeHeader("Cache-Control", cache_header);
    res->writeStatus("200 OK");
    res->end();
  });
}

}  // namespace Core::HttpServer::Static