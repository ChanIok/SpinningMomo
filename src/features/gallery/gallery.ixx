module;

export module Features.Gallery;

import std;
import Core.State;
import Features.Gallery.Types;

export namespace Features::Gallery {

// ============= 初始化和清理 =============

// 初始化gallery模块
auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

// 清理gallery模块资源
auto cleanup(Core::State::AppState& app_state) -> void;

// ============= 资产项管理 =============

// 删除资产项
auto delete_asset(Core::State::AppState& app_state, const Types::DeleteParams& params)
    -> std::expected<Types::OperationResult, std::string>;

// ============= 扫描和索引 =============

// 扫描资产目录并建立索引（支持文件夹和忽略规则）
auto scan_directory(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string>;

// ============= 缩略图管理 =============

// 清理孤立的缩略图文件
auto cleanup_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::OperationResult, std::string>;

// ============= 统计和信息 =============

// 获取缩略图统计信息
auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string>;  // 返回格式化的统计信息字符串

// ============= 维护和优化 =============

// 数据库维护：清理软删除的记录
auto cleanup_deleted_assets(Core::State::AppState& app_state, int days_old = 30)
    -> std::expected<Types::OperationResult, std::string>;

}  // namespace Features::Gallery
