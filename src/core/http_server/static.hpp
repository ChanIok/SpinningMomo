#pragma once

#include "vendor/std.hpp"

#include "vendor/uwebsockets.hpp"

#include "core/http_server/state.hpp"
#include "core/http_server/types.hpp"
#include "core/state/app_state.hpp"

namespace core::http_server::static_content {

// 读取并发送下一个数据块
auto read_and_send_next_chunk(std::shared_ptr<StreamContext> ctx) -> void;

// 注册自定义路径解析器（接受 AppState）
auto register_path_resolver(core::AppState& state, std::string prefix, PathResolver resolver)
    -> void;

// 注销路径解析器
auto unregister_path_resolver(core::AppState& state, std::string_view prefix) -> void;

// 注册静态文件路由（作为fallback）
auto register_routes(core::AppState& state, uWS::App& app) -> void;

// 以附件形式发送一个已经解析并校验过的文件，复用静态服务的异步读取和流式发送。
// 可传入 stream_lifetime_token 绑定生命周期卫士，响应销毁时自动析构。
auto serve_download_file_request(core::AppState& state, const std::filesystem::path& file_path,
                                 std::string download_name, uWS::HttpResponse<false>* res,
                                 uWS::HttpRequest* req, bool allow_range = true,
                                 std::shared_ptr<void> stream_lifetime_token = nullptr) -> void;

}  // namespace core::http_server::static_content
