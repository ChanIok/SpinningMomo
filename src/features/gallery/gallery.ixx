module;

export module Features.Gallery;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery {

// 初始化与清理
export auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;
export auto cleanup(Core::State::AppState& app_state) -> void;

// 资产管理
export auto delete_asset(Core::State::AppState& app_state, const Types::DeleteParams& params)
    -> std::expected<Types::OperationResult, std::string>;

// 扫描与索引
export auto scan_directory(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string>;

// 缩略图
export auto cleanup_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::OperationResult, std::string>;

// 统计
export auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string>;

}  // namespace Features::Gallery
