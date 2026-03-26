module;

module Features.Gallery.StaticResolver;

import std;
import Core.State;
import Core.HttpServer.Static;
import Core.HttpServer.Types;
import Core.WebView;
import Core.WebView.Static;
import Core.WebView.State;
import Core.WebView.Types;
import Features.Gallery.State;
import Features.Gallery.OriginalLocator;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Vendor.BuildConfig;

namespace Features::Gallery::StaticResolver {

// 从 URL 提取相对路径（通用模板版本）
template <typename CharT>
auto extract_relative_path_generic(std::basic_string_view<CharT> url,
                                   std::basic_string_view<CharT> prefix)
    -> std::optional<std::basic_string<CharT>> {
  if (!url.starts_with(prefix)) {
    return std::nullopt;
  }

  auto relative = url.substr(prefix.size());
  if (relative.empty()) {
    return std::nullopt;
  }

  // 去掉前导 '/'
  if (relative.front() == static_cast<CharT>('/')) {
    relative = relative.substr(1);
  }

  // 去掉查询参数和 fragment（找到第一个出现的位置）
  auto end_pos =
      std::min(relative.find(static_cast<CharT>('?')), relative.find(static_cast<CharT>('#')));
  if (end_pos != std::basic_string_view<CharT>::npos) {
    relative = relative.substr(0, end_pos);
  }

  if (relative.empty()) {
    return std::nullopt;
  }

  return std::basic_string<CharT>(relative);
}

// 显示实例化模板函数
template auto extract_relative_path_generic<char>(std::basic_string_view<char>,
                                                  std::basic_string_view<char>)
    -> std::optional<std::basic_string<char>>;
template auto extract_relative_path_generic<wchar_t>(std::basic_string_view<wchar_t>,
                                                     std::basic_string_view<wchar_t>)
    -> std::optional<std::basic_string<wchar_t>>;

// 从 URL 提取相对路径（narrow 版本）
auto extract_relative_path(std::string_view url, std::string_view prefix)
    -> std::optional<std::string> {
  return extract_relative_path_generic(url, prefix);
}

// 从 URL 提取相对路径（wide 版本）
auto extract_relative_path_w(std::wstring_view url, std::wstring_view prefix)
    -> std::optional<std::wstring> {
  return extract_relative_path_generic(url, prefix);
}

// 把 URL 中的 %XX 编码还原成原始字节串。
// 因为 relative_path 可能包含中文、空格、#、% 等字符，所以 dev 路由必须先解码。
auto decode_percent_encoded_string(std::string_view input) -> std::optional<std::string> {
  std::string decoded;
  decoded.reserve(input.size());

  for (std::size_t index = 0; index < input.size(); ++index) {
    auto ch = input[index];
    if (ch == '%') {
      if (index + 2 >= input.size()) {
        return std::nullopt;
      }

      unsigned int value = 0;
      auto hex = input.substr(index + 1, 2);
      auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), value, 16);
      if (ec != std::errc{} || ptr != hex.data() + hex.size()) {
        return std::nullopt;
      }

      decoded.push_back(static_cast<char>(value));
      index += 2;
      continue;
    }

    decoded.push_back(ch == '+' ? ' ' : ch);
  }

  return decoded;
}

struct OriginalRequestLocator {
  std::int64_t root_id = 0;
  std::string relative_path;
};

// 从 dev HTTP originals 路由中提取 root_id 和 relative_path。
// 路径形态是：/static/assets/originals/by-root/<rootId>/<relativePath>?v=<hash>
auto extract_original_request_locator(std::string_view url, std::string_view prefix)
    -> std::optional<OriginalRequestLocator> {
  auto relative = extract_relative_path(url, prefix);
  if (!relative) {
    return std::nullopt;
  }

  auto separator = relative->find('/');
  if (separator == std::string::npos) {
    return std::nullopt;
  }

  std::int64_t root_id = 0;
  auto root_id_part = std::string_view(*relative).substr(0, separator);
  auto [ptr, ec] =
      std::from_chars(root_id_part.data(), root_id_part.data() + root_id_part.size(), root_id);
  if (ec != std::errc{} || ptr != root_id_part.data() + root_id_part.size() || root_id <= 0) {
    return std::nullopt;
  }

  auto encoded_relative_path = std::string_view(*relative).substr(separator + 1);
  if (encoded_relative_path.empty()) {
    return std::nullopt;
  }

  auto decoded_relative_path = decode_percent_encoded_string(encoded_relative_path);
  if (!decoded_relative_path || decoded_relative_path->empty()) {
    return std::nullopt;
  }

  return OriginalRequestLocator{.root_id = root_id,
                                .relative_path = std::move(*decoded_relative_path)};
}

// resolver 用的最小文件校验。
// 这里不做业务层判断，只确认文件存在且是普通文件。
auto validate_asset_file(const std::filesystem::path& path) -> bool {
  std::error_code ec;
  return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec) && !ec;
}

// ============= HTTP 静态服务解析器 =============

auto register_http_resolvers(Core::State::AppState& state) -> void {
  // 缩略图解析器
  Core::HttpServer::Static::register_path_resolver(
      state,  // 传递 AppState
      "/static/assets/thumbnails/",
      [&state](std::string_view url_path) -> Core::HttpServer::Types::PathResolution {
        auto relative_path = extract_relative_path(url_path, "/static/assets/thumbnails/");
        if (!relative_path) {
          return std::unexpected("Invalid thumbnail path");
        }

        if (!state.gallery || state.gallery->thumbnails_directory.empty()) {
          return std::unexpected("Thumbnails directory not initialized");
        }

        auto full_path = state.gallery->thumbnails_directory / *relative_path;

        if (!Utils::Path::IsPathWithinBase(full_path, state.gallery->thumbnails_directory)) {
          Logger().warn("Unsafe thumbnail path requested: {}", full_path.string());
          return std::unexpected("Unsafe thumbnail path");
        }
        if (!validate_asset_file(full_path)) {
          Logger().debug("Thumbnail file not found: {}", full_path.string());
          return std::unexpected("Thumbnail file not found");
        }

        Logger().debug("Resolved thumbnail path: {}", full_path.string());
        return Core::HttpServer::Types::PathResolutionData{
            .file_path = full_path,
            .cache_duration = std::chrono::seconds{86400},
            .cache_control_header = std::string{"public, max-age=31536000, immutable"}};
      });

  // 原图解析器（基于 root_id + relative_path）。
  // 这条链路主要给浏览器 dev 环境使用；release WebView 会尽量直接走 root host mapping。
  Core::HttpServer::Static::register_path_resolver(
      state, "/static/assets/originals/by-root/",
      [&state](std::string_view url_path) -> Core::HttpServer::Types::PathResolution {
        auto locator =
            extract_original_request_locator(url_path, "/static/assets/originals/by-root/");
        if (!locator) {
          return std::unexpected("Invalid original locator");
        }

        auto path_result = Features::Gallery::OriginalLocator::resolve_original_file_path(
            state, locator->root_id, locator->relative_path);
        if (!path_result) {
          Logger().warn("Failed to resolve original locator {}/{}: {}", locator->root_id,
                        locator->relative_path, path_result.error());
          return std::unexpected(path_result.error());
        }

        if (!validate_asset_file(*path_result)) {
          Logger().warn("Original file not found: {}", path_result->string());
          return std::unexpected("Asset file not found");
        }

        Logger().debug("Resolved original locator {}/{} to {}", locator->root_id,
                       locator->relative_path, path_result->string());
        return Core::HttpServer::Types::PathResolutionData{
            .file_path = *path_result,
            .cache_duration = std::chrono::seconds{0},
            .cache_control_header = std::string{"private, no-cache"}};
      });

  Logger().info("Registered HTTP static resolvers for gallery");
}

// ============= WebView 资源解析器 =============

auto register_webview_resolvers(Core::State::AppState& state) -> void {
  if (!state.webview) {
    Logger().warn("WebView state not initialized, skipping resolver registration");
    return;
  }

  if (!state.gallery || state.gallery->thumbnails_directory.empty()) {
    Logger().warn("Thumbnails directory not initialized, skipping WebView thumbnail setup");
    return;
  }

  if (!Vendor::BuildConfig::is_debug_build()) {
    // Release WebView 直接把整个缩略图目录映射成独立 host。
    // 这样 `<img src>` 会直接从文件夹读取，不再绕回 static.test 的动态拦截链路。
    Core::WebView::register_virtual_host_folder_mapping(
        state, state.webview->config.thumbnail_host_name,
        state.gallery->thumbnails_directory.wstring(),
        Core::WebView::State::VirtualHostResourceAccessKind::allow);

    Logger().info("Registered WebView thumbnail host mapping: {} -> {}",
                  Utils::String::ToUtf8(state.webview->config.thumbnail_host_name),
                  state.gallery->thumbnails_directory.string());
    return;
  }

  auto thumbnail_prefix = state.webview->config.dev_server_url + L"/static/assets/thumbnails/";
  Core::WebView::Static::register_web_resource_resolver(
      state, thumbnail_prefix,
      [&state,
       thumbnail_prefix](std::wstring_view url) -> Core::WebView::Types::WebResourceResolution {
        auto relative_path = extract_relative_path_w(url, thumbnail_prefix);
        if (!relative_path) {
          return {.success = false, .file_path = {}, .error_message = "Invalid thumbnail URL"};
        }

        auto full_path =
            state.gallery->thumbnails_directory / std::filesystem::path(*relative_path);

        if (!Utils::Path::IsPathWithinBase(full_path, state.gallery->thumbnails_directory)) {
          Logger().warn("Unsafe thumbnail path requested via WebView: {}", full_path.string());
          return {.success = false, .file_path = {}, .error_message = "Unsafe thumbnail path"};
        }
        if (!validate_asset_file(full_path)) {
          Logger().debug("Thumbnail file not found via WebView: {}", full_path.string());
          return {.success = false, .file_path = {}, .error_message = "Thumbnail file not found"};
        }

        return {.success = true,
                .file_path = full_path,
                .error_message = {},
                .content_type = L"image/webp",
                .status_code = 200,
                .cache_control_header = L"public, max-age=31536000, immutable"};
      });

  Logger().info("Registered debug WebView thumbnail resolver: {}",
                Utils::String::ToUtf8(thumbnail_prefix));
}
// ============= 清理函数 =============

auto unregister_all_resolvers(Core::State::AppState& state) -> void {
  Core::HttpServer::Static::unregister_path_resolver(state, "/static/assets/thumbnails/");
  Core::HttpServer::Static::unregister_path_resolver(state, "/static/assets/originals/by-root/");

  if (state.webview) {
    Core::WebView::unregister_virtual_host_folder_mapping(
        state, state.webview->config.thumbnail_host_name);
  }

  Logger().info("Unregistered gallery static resolvers");
}
}  // namespace Features::Gallery::StaticResolver
