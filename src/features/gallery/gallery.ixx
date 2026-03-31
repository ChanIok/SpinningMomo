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
export auto open_asset_with_default_app(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string>;
export auto reveal_asset_in_explorer(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<Types::OperationResult, std::string>;
export auto move_assets_to_trash(Core::State::AppState& app_state,
                                 const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string>;

// 扫描与索引
export auto scan_directory(Core::State::AppState& app_state, const Types::ScanOptions& options,
                           std::function<void(const Types::ScanProgress&)> progress_callback =
                               nullptr) -> std::expected<Types::ScanResult, std::string>;
export auto ensure_output_directory_media_source(Core::State::AppState& app_state,
                                                 const std::string& output_dir_path) -> void;

// 缩略图
export auto cleanup_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::OperationResult, std::string>;

// 统计
export auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string>;

}  // namespace Features::Gallery
