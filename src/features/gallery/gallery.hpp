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

// 缩略图
auto cleanup_thumbnails(core::AppState& app_state) -> std::expected<OperationResult, std::string>;

// 统计
auto get_thumbnail_stats(core::AppState& app_state) -> std::expected<std::string, std::string>;

}  // namespace features::gallery
