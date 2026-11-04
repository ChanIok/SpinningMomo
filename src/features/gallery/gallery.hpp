#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery {

// 初始化与清理
auto initialize(core::AppState& app_state,
                std::function<void(core::AppState&)> after_ready = nullptr)
    -> std::expected<void, std::string>;
auto cleanup(core::AppState& app_state,
             std::function<void(core::AppState&)> before_watchers_shutdown = nullptr) -> void;

// 扫描与索引
auto scan_directory(core::AppState& app_state, const ScanOptions& options,
                    std::function<void(const ScanProgress&)> progress_callback = nullptr)
    -> std::expected<ScanResult, std::string>;
auto ensure_output_directory_media_source(core::AppState& app_state,
                                          const std::string& output_dir_path) -> void;

// 将已关闭的文件同步到已有图库根，按需补齐子目录；不属于图库时返回空变化。
// 不注册监控根、不管理路径忽略；调用方结束忽略后再分发返回的变化。
// 目录已提交后资产失败记入 errors，仍须发布已创建目录的事实。
auto index_created_file(core::AppState& app_state, const std::filesystem::path& path)
    -> std::expected<ScanResult, std::string>;

// 分发已经落库的文件及目录变化，并通知图库刷新。
auto publish_scan_result(core::AppState& app_state, const ScanResult& result)
    -> std::expected<void, std::string>;

// 配对已 begin 的创建操作：同步索引、结束路径忽略，再发布变化。文件必须已关闭。
auto complete_created_file_operation(core::AppState& app_state, const std::filesystem::path& path)
    -> std::expected<void, std::string>;

// 缩略图
auto cleanup_thumbnails(core::AppState& app_state) -> std::expected<OperationResult, std::string>;

// 统计
auto get_thumbnail_stats(core::AppState& app_state) -> std::expected<std::string, std::string>;

}  // namespace features::gallery
