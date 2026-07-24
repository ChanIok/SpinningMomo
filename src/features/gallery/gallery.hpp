#pragma once

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery {

// 初始化与清理
auto initialize(Core::State::AppState& app_state,
                std::function<void(Core::State::AppState&)> after_ready = nullptr)
    -> std::expected<void, std::string>;
auto cleanup(Core::State::AppState& app_state,
             std::function<void(Core::State::AppState&)> before_watchers_shutdown = nullptr)
    -> void;

// 扫描与索引
auto scan_directory(Core::State::AppState& app_state, const Types::ScanOptions& options,
                    std::function<void(const Types::ScanProgress&)> progress_callback = nullptr)
    -> std::expected<Types::ScanResult, std::string>;
auto ensure_output_directory_media_source(Core::State::AppState& app_state,
                                          const std::string& output_dir_path) -> void;

// 缩略图
auto cleanup_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::OperationResult, std::string>;

// 统计
auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string>;

}  // namespace Features::Gallery
