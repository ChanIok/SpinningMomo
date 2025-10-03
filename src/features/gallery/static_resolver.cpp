module;

module Features.Gallery.StaticResolver;

import std;
import Core.State;
import Core.HttpServer.Static;
import Core.HttpServer.Types;
import Core.WebView.Static;
import Core.WebView.State;
import Features.Gallery.State;
import Utils.Logger;
import Utils.String;
import Vendor.BuildConfig;

namespace Features::Gallery::StaticResolver {

// ============= 内部辅助函数 =============

namespace {

// 从 URL 提取相对路径（narrow 版本）
auto extract_relative_path(std::string_view url, std::string_view prefix)
    -> std::optional<std::string> {
  if (!url.starts_with(prefix)) {
    return std::nullopt;
  }

  auto relative = url.substr(prefix.size());
  if (relative.empty()) {
    return std::nullopt;
  }

  // 去掉前导 '/'
  if (relative.front() == '/') {
    relative = relative.substr(1);
  }

  // 去掉查询参数和 fragment
  if (auto pos = relative.find('?'); pos != std::string_view::npos) {
    relative = relative.substr(0, pos);
  }
  if (auto pos = relative.find('#'); pos != std::string_view::npos) {
    relative = relative.substr(0, pos);
  }

  if (relative.empty()) {
    return std::nullopt;
  }

  return std::string(relative);
}

// 从 URL 提取相对路径（wide 版本）
auto extract_relative_path_w(std::wstring_view url, std::wstring_view prefix)
    -> std::optional<std::wstring> {
  if (!url.starts_with(prefix)) {
    return std::nullopt;
  }

  auto relative = url.substr(prefix.size());
  if (relative.empty()) {
    return std::nullopt;
  }

  if (relative.front() == L'/') {
    relative = relative.substr(1);
  }

  if (auto pos = relative.find(L'?'); pos != std::wstring_view::npos) {
    relative = relative.substr(0, pos);
  }
  if (auto pos = relative.find(L'#'); pos != std::wstring_view::npos) {
    relative = relative.substr(0, pos);
  }

  if (relative.empty()) {
    return std::nullopt;
  }

  return std::wstring(relative);
}

// 路径安全检查
auto is_path_safe(const std::filesystem::path& target, const std::filesystem::path& base) -> bool {
  try {
    std::error_code ec;
    auto normalized_base = std::filesystem::weakly_canonical(base, ec);
    if (ec) return false;

    auto normalized_target = std::filesystem::weakly_canonical(target, ec);
    if (ec) return false;

    auto relative = std::filesystem::relative(normalized_target, normalized_base, ec);
    if (ec || relative.native().starts_with(L"..")) {
      return false;
    }

    return std::filesystem::exists(normalized_target);
  } catch (...) {
    return false;
  }
}

}  // anonymous namespace

// ============= HTTP 静态服务解析器 =============

auto register_http_resolvers(Core::State::AppState& state) -> void {
  // 缩略图解析器
  Core::HttpServer::Static::register_path_resolver(
      state,  // 传递 AppState
      "/static/thumbnails/",
      [&state](std::string_view url_path) -> Core::HttpServer::Types::PathResolution {
        auto relative_path = extract_relative_path(url_path, "/static/thumbnails/");
        if (!relative_path) {
          return std::unexpected("Invalid thumbnail path");
        }

        if (!state.gallery || state.gallery->thumbnails_directory.empty()) {
          return std::unexpected("Thumbnails directory not initialized");
        }

        auto full_path = state.gallery->thumbnails_directory / *relative_path;

        if (!is_path_safe(full_path, state.gallery->thumbnails_directory)) {
          Logger().warn("Unsafe thumbnail path requested: {}", full_path.string());
          return std::unexpected("Unsafe path or file not found");
        }

        Logger().debug("Resolved thumbnail path: {}", full_path.string());
        return Core::HttpServer::Types::PathResolutionData{
            .file_path = full_path, .cache_duration = std::chrono::seconds{86400}  // 1天缓存
        };
      });

  // 原图解析器（预留，暂未实现）
  Core::HttpServer::Static::register_path_resolver(
      state,  // 传递 AppState
      "/static/images/",
      [&state](std::string_view url_path) -> Core::HttpServer::Types::PathResolution {
        // TODO: 实现原图解析逻辑
        // 可能需要从数据库查询 Asset 获取实际文件路径

        return std::unexpected("Original image serving not implemented yet");
      });

  Logger().info("Registered HTTP static resolvers for gallery");
}

// ============= WebView 资源解析器 =============

auto register_webview_resolvers(Core::State::AppState& state) -> void {
  if (!state.webview) {
    Logger().warn("WebView state not initialized, skipping resolver registration");
    return;
  }

  // 根据 debug/release 模式确定前缀
  std::wstring thumbnail_prefix;
  if (Vendor::BuildConfig::is_debug_build()) {
    // Debug 模式：拦截开发服务器的路径
    thumbnail_prefix = state.webview->config.dev_server_url + L"/static/thumbnails/";
  } else {
    // Release 模式：拦截虚拟主机的路径
    thumbnail_prefix =
        L"https://" + state.webview->config.static_host_name + L"/static/thumbnails/";
  }

  Core::WebView::Static::register_web_resource_resolver(
      state,  // 传递 AppState
      thumbnail_prefix,
      [&state, thumbnail_prefix](std::wstring_view url)
          -> Core::WebView::Types::WebResourceResolution {
        auto relative_path = extract_relative_path_w(url, thumbnail_prefix);
        if (!relative_path) {
          return {.success = false, .file_path = {}, .error_message = "Invalid thumbnail URL"};
        }

        if (!state.gallery || state.gallery->thumbnails_directory.empty()) {
          return {.success = false,
                  .file_path = {},
                  .error_message = "Thumbnails directory not initialized"};
        }

        auto full_path =
            state.gallery->thumbnails_directory / std::filesystem::path(*relative_path);

        if (!is_path_safe(full_path, state.gallery->thumbnails_directory)) {
          Logger().warn("Unsafe thumbnail path requested via WebView: {}", full_path.string());
          return {
              .success = false, .file_path = {}, .error_message = "Unsafe path or file not found"};
        }

        Logger().debug("Resolved WebView thumbnail: {}", full_path.string());
        return {.success = true,
                .file_path = full_path,
                .error_message = {},
                .content_type = L"image/webp",
                .status_code = 200};
      });

  Logger().info("Registered WebView resource resolvers for gallery (prefix: {})",
                Utils::String::ToUtf8(thumbnail_prefix));
}

// ============= 清理函数 =============

auto unregister_all_resolvers(Core::State::AppState& state) -> void {
  Core::HttpServer::Static::unregister_path_resolver(state, "/static/thumbnails/");
  Core::HttpServer::Static::unregister_path_resolver(state, "/static/images/");

  // WebView 注销需要运行时确定前缀
  // 目前简化处理，可以考虑在 WebView 模块提供 unregister_all 接口

  Logger().info("Unregistered gallery static resolvers");
}

}  // namespace Features::Gallery::StaticResolver
