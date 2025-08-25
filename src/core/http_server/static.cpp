module;

#include <uwebsockets/App.h>

#include <asio.hpp>

module Core.HttpServer.Static;

import std;
import Core.State;
import Core.Async.Runtime;
import Utils.File;
import Utils.File.Mime;
import Utils.Path;
import Utils.Logger;

namespace Core::HttpServer::Static {

// 安全的路径检查
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

// 处理静态文件请求
auto handle_static_request(Core::State::AppState& state, const std::string& url_path, auto* res,
                           auto* req) -> void {
  // 解析文件路径
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

  Logger().debug("Serving static file: {}", file_path.string());

  // 获取当前的事件循环
  auto* loop = uWS::Loop::get();

  // 在异步运行时中处理文件读取
  asio::co_spawn(
      *Core::Async::get_io_context(*state.async_runtime),
      [res, file_path, loop]() -> asio::awaitable<void> {
        try {
          // 检查文件是否存在
          if (!std::filesystem::exists(file_path)) {
            Logger().warn("File not found: {}", file_path.string());
            loop->defer([res]() {
              res->writeStatus("404 Not Found");
              res->end("File not found");
            });
            co_return;
          }

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
          loop->defer([res, file_path, file_data]() mutable {
            // 设置Content-Type
            res->writeHeader("Content-Type", file_data.mime_type);

            // 设置缓存头
            auto extension = file_path.extension().string();
            auto cache_duration = get_cache_duration(extension);
            auto cache_header = std::format("public, max-age={}", cache_duration.count());
            res->writeHeader("Cache-Control", cache_header);

            // 设置其他安全相关的头
            res->writeHeader("X-Content-Type-Options", "nosniff");

            // 对于文本文件，设置字符编码
            if (file_data.mime_type.starts_with("text/") ||
                file_data.mime_type.contains("charset=")) {
              if (!file_data.mime_type.contains("charset=")) {
                res->writeHeader("Content-Type", file_data.mime_type + "; charset=utf-8");
              }
            }

            res->writeStatus("200 OK");
            res->end(std::string(file_data.data.begin(), file_data.data.end()));

            Logger().debug("Served file: {}, size: {} bytes", file_path.string(),
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