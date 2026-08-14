#include "core/http_server/downloads.hpp"

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/http_server/access.hpp"
#include "core/http_server/static.hpp"
#include "features/gallery/download/download.hpp"
#include "utils/logger/logger.hpp"

namespace core::http_server::downloads {
namespace {

// 返回统一的下载鉴权失败响应。
auto reject_unauthorized(auto* res) -> void {
  res->writeStatus("401 Unauthorized");
  res->writeHeader("Cache-Control", "no-store");
  res->writeHeader("Content-Type", "text/plain; charset=utf-8");
  res->end("Authentication required");
}

// 返回不暴露磁盘路径的下载不存在响应。
auto reject_not_found(auto* res) -> void {
  res->writeStatus("404 Not Found");
  res->writeHeader("Cache-Control", "no-store");
  res->end("Download not found");
}

// 将 URL 参数解析成正整数资产 ID。
auto resolve_asset_id(std::string_view value) -> std::optional<std::int64_t> {
  if (value.empty()) {
    return std::nullopt;
  }

  std::int64_t asset_id = 0;
  const auto [pointer, error] =
      std::from_chars(value.data(), value.data() + value.size(), asset_id);
  if (error != std::errc{} || pointer != value.data() + value.size() || asset_id <= 0) {
    return std::nullopt;
  }
  return asset_id;
}

// 校验当前请求是否拥有本机或局域网下载权限。
auto has_download_access(core::AppState& state, auto* res, auto* req) -> bool {
  if (core::http_server::access::resolve_http_access(state, res->getRemoteAddressAsText(),
                                                     req->getHeader("cookie"))) {
    return true;
  }
  reject_unauthorized(res);
  return false;
}

// 解析资产 ID 后发送仍存在的原始媒体文件。
auto handle_asset_download(core::AppState& state, auto* res, auto* req) -> void {
  if (!has_download_access(state, res, req)) {
    return;
  }

  // 只接受正整数 ID，避免把 URL 参数直接当成路径使用。
  const auto asset_id = resolve_asset_id(req->getParameter("asset_id"));
  if (!asset_id) {
    reject_not_found(res);
    return;
  }

  // 根据资产记录重新解析磁盘路径，下载路由不信任客户端传入的文件路径。
  auto file_result = features::gallery::download::resolve_asset_file(state, *asset_id);
  if (!file_result) {
    Logger().debug("Gallery asset download was not found for {}: {}", *asset_id,
                   file_result.error());
    reject_not_found(res);
    return;
  }

  static_content::serve_download_file_request(state, file_result->file_path,
                                              std::move(file_result->file_name), res, req);
}

// 发送完整的一次性 ZIP，并在响应结束后删除归档文件。
auto handle_archive_download(core::AppState& state, auto* res, auto* req) -> void {
  if (!has_download_access(state, res, req)) {
    return;
  }

  auto file_result =
      features::gallery::download::resolve_archive_file(state, req->getParameter("archive_name"));
  if (!file_result) {
    Logger().debug("Gallery archive download was not found: {}", file_result.error());
    reject_not_found(res);
    return;
  }

  // 归档不支持 Range；只有完整响应结束才触发删除回调。
  const auto archive_path = file_result->file_path;
  static_content::serve_download_file_request(
      state, file_result->file_path, std::move(file_result->file_name), res, req, false,
      [archive_path] { features::gallery::download::remove_archive_file(archive_path); });
}

}  // namespace

// 将两类图库下载路径接入 HTTP 服务。
auto register_routes(core::AppState& state, uWS::App& app) -> void {
  Logger().info("Registering gallery download routes");
  app.get("/downloads/assets/:asset_id",
          [&state](auto* res, auto* req) { handle_asset_download(state, res, req); });
  app.get("/downloads/archives/:archive_name",
          [&state](auto* res, auto* req) { handle_archive_download(state, res, req); });
}

}  // namespace core::http_server::downloads
