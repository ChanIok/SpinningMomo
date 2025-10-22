module;

module Features.Gallery.StaticResolver;

import std;
import Core.State;
import Core.HttpServer.Static;
import Core.HttpServer.Types;
import Core.WebView.Static;
import Core.WebView.State;
import Core.WebView.Types;
import Features.Gallery.State;
import Features.Gallery.Asset.Repository;
import Utils.Logger;
import Utils.String;
import Utils.LRUCache;
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

// 从 URL 提取 asset_id（通用模板版本）
template <typename CharT>
auto extract_asset_id_generic(std::basic_string_view<CharT> url,
                              std::basic_string_view<CharT> prefix) -> std::optional<std::int64_t> {
  if (!url.starts_with(prefix)) {
    return std::nullopt;
  }

  auto id_str = url.substr(prefix.size());
  if (id_str.empty() || id_str.front() == static_cast<CharT>('/')) {
    id_str = id_str.substr(1);
  }

  // 去掉扩展名和查询参数（找到第一个出现的位置）
  auto end_pos =
      std::min(id_str.find(static_cast<CharT>('.')), id_str.find(static_cast<CharT>('?')));
  if (end_pos != std::basic_string_view<CharT>::npos) {
    id_str = id_str.substr(0, end_pos);
  }

  if (id_str.empty()) {
    return std::nullopt;
  }

  // 使用 std::from_chars 避免字符串分配（仅对 char 类型）
  if constexpr (std::same_as<CharT, char>) {
    std::int64_t result;
    auto [ptr, ec] = std::from_chars(id_str.data(), id_str.data() + id_str.size(), result);
    if (ec == std::errc{} && ptr == id_str.data() + id_str.size()) {
      return result;
    }
    return std::nullopt;
  } else {
    // wchar_t 版本使用传统方式
    try {
      return std::stoll(std::basic_string<CharT>(id_str));
    } catch (...) {
      return std::nullopt;
    }
  }
}

// 显示实例化模板函数
template auto extract_relative_path_generic<char>(std::basic_string_view<char>,
                                                  std::basic_string_view<char>)
    -> std::optional<std::basic_string<char>>;
template auto extract_relative_path_generic<wchar_t>(std::basic_string_view<wchar_t>,
                                                     std::basic_string_view<wchar_t>)
    -> std::optional<std::basic_string<wchar_t>>;

template auto extract_asset_id_generic<char>(std::basic_string_view<char>,
                                             std::basic_string_view<char>)
    -> std::optional<std::int64_t>;
template auto extract_asset_id_generic<wchar_t>(std::basic_string_view<wchar_t>,
                                                std::basic_string_view<wchar_t>)
    -> std::optional<std::int64_t>;

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

// 从 URL 提取 asset_id（narrow 版本）
auto extract_asset_id(std::string_view url, std::string_view prefix)
    -> std::optional<std::int64_t> {
  return extract_asset_id_generic(url, prefix);
}

// 从 URL 提取 asset_id（wide 版本）
auto extract_asset_id_w(std::wstring_view url, std::wstring_view prefix)
    -> std::optional<std::int64_t> {
  return extract_asset_id_generic(url, prefix);
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

// 简化的文件验证（只检查存在性和可读性）
auto validate_asset_file(const std::filesystem::path& path) -> bool {
  std::error_code ec;
  return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec) && !ec;
}

// 从数据库查询 asset 路径
auto get_asset_path_from_db(Core::State::AppState& state, std::int64_t asset_id)
    -> std::optional<std::filesystem::path> {
  auto asset_result = Features::Gallery::Asset::Repository::get_asset_by_id(state, asset_id);
  if (!asset_result || !asset_result->has_value()) {
    return std::nullopt;
  }

  return std::filesystem::path(asset_result->value().path);
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

        if (!is_path_safe(full_path, state.gallery->thumbnails_directory)) {
          Logger().warn("Unsafe thumbnail path requested: {}", full_path.string());
          return std::unexpected("Unsafe path or file not found");
        }

        Logger().debug("Resolved thumbnail path: {}", full_path.string());
        return Core::HttpServer::Types::PathResolutionData{
            .file_path = full_path, .cache_duration = std::chrono::seconds{86400}  // 1天缓存
        };
      });

  // 原图解析器（基于 asset_id）
  Core::HttpServer::Static::register_path_resolver(
      state, "/static/assets/originals",
      [&state](std::string_view url_path) -> Core::HttpServer::Types::PathResolution {
        auto asset_id = extract_asset_id(url_path, "/static/assets/originals");
        if (!asset_id) {
          return std::unexpected("Invalid asset ID");
        }

        // 1. 先查缓存
        if (state.gallery) {
          auto cached_path = Utils::LRUCache::get(state.gallery->image_path_cache, *asset_id);
          if (cached_path) {
            if (validate_asset_file(*cached_path)) {
              Logger().debug("Cache hit for asset {}: {}", *asset_id, cached_path->string());
              return Core::HttpServer::Types::PathResolutionData{
                  .file_path = *cached_path, .cache_duration = std::chrono::seconds{86400}
                  // 1天缓存
              };
            } else {
              Logger().warn("Cached path for asset {} not found: {}", *asset_id,
                            cached_path->string());
            }
          }
        }

        // 2. 缓存未命中，查数据库
        auto path = get_asset_path_from_db(state, *asset_id);
        if (!path) {
          Logger().warn("Asset not found in database: {}", *asset_id);
          return std::unexpected("Asset not found");
        }

        if (!validate_asset_file(*path)) {
          Logger().warn("Asset file not found: {}", path->string());
          return std::unexpected("Asset file not found");
        }

        // 3. 更新缓存
        if (state.gallery) {
          Utils::LRUCache::put(state.gallery->image_path_cache, *asset_id, *path);
        }

        Logger().debug("Resolved asset {} from database: {}", *asset_id, path->string());
        return Core::HttpServer::Types::PathResolutionData{
            .file_path = *path, .cache_duration = std::chrono::seconds{86400}};
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
    thumbnail_prefix = state.webview->config.dev_server_url + L"/static/assets/thumbnails/";
  } else {
    // Release 模式：拦截虚拟主机的路径
    thumbnail_prefix =
        L"https://" + state.webview->config.static_host_name + L"/static/assets/thumbnails/";
  }

  Core::WebView::Static::register_web_resource_resolver(
      state,  // 传递 AppState
      thumbnail_prefix,
      [&state,
       thumbnail_prefix](std::wstring_view url) -> Core::WebView::Types::WebResourceResolution {
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

  // 原图解析器（基于 asset_id）
  std::wstring image_prefix;
  if (Vendor::BuildConfig::is_debug_build()) {
    image_prefix = state.webview->config.dev_server_url + L"/static/assets/originals";
  } else {
    image_prefix = L"https://" + state.webview->config.static_host_name + L"/static/assets/originals";
  }

  Core::WebView::Static::register_web_resource_resolver(
      state, image_prefix,
      [&state, image_prefix](std::wstring_view url) -> Core::WebView::Types::WebResourceResolution {
        auto asset_id = extract_asset_id_w(url, image_prefix);
        if (!asset_id) {
          return {.success = false, .file_path = {}, .error_message = "Invalid asset ID"};
        }

        // 1. 先查缓存
        if (state.gallery) {
          auto cached_path = Utils::LRUCache::get(state.gallery->image_path_cache, *asset_id);
          if (cached_path) {
            if (validate_asset_file(*cached_path)) {
              Logger().debug("Cache hit for WebView asset {}: {}", *asset_id,
                             cached_path->string());
              return {.success = true,
                      .file_path = *cached_path,
                      .error_message = {},
                      .content_type = std::nullopt,  // 自动检测
                      .status_code = 200};
            } else {
              Logger().warn("Cached path for WebView asset {} not found: {}", *asset_id,
                            cached_path->string());
            }
          }
        }

        // 2. 缓存未命中，查数据库
        auto path = get_asset_path_from_db(state, *asset_id);
        if (!path) {
          Logger().warn("WebView asset not found in database: {}", *asset_id);
          return {.success = false, .file_path = {}, .error_message = "Asset not found"};
        }

        if (!validate_asset_file(*path)) {
          Logger().warn("WebView asset file not found: {}", path->string());
          return {.success = false, .file_path = {}, .error_message = "Asset file not found"};
        }

        // 3. 更新缓存
        if (state.gallery) {
          Utils::LRUCache::put(state.gallery->image_path_cache, *asset_id, *path);
        }

        Logger().debug("Resolved WebView asset {} from database: {}", *asset_id, path->string());
        return {.success = true,
                .file_path = *path,
                .error_message = {},
                .content_type = std::nullopt,  // 自动检测
                .status_code = 200};
      });

  Logger().info("Registered WebView resource resolvers for gallery (thumbnails: {}, images: {})",
                Utils::String::ToUtf8(thumbnail_prefix), Utils::String::ToUtf8(image_prefix));
}

// ============= 清理函数 =============

auto unregister_all_resolvers(Core::State::AppState& state) -> void {
  Core::HttpServer::Static::unregister_path_resolver(state, "/static/assets/thumbnails/");
  Core::HttpServer::Static::unregister_path_resolver(state, "/static/assets/originals");

  // WebView 注销需要运行时确定前缀
  // 目前简化处理，可以考虑在 WebView 模块提供 unregister_all 接口

  Logger().info("Unregistered gallery static resolvers");
}

}  // namespace Features::Gallery::StaticResolver
