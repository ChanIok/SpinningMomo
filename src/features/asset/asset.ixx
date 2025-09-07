module;

export module Features.Asset;

import std;
import Core.State;
import Features.Asset.Types;

export namespace Features::Asset {

// ============= 初始化和清理 =============

// 初始化资产模块
auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

// 清理资产模块资源
auto cleanup(Core::State::AppState& app_state) -> void;

// ============= 资产项管理 =============

// 删除资产项
auto delete_item(Core::State::AppState& app_state, const Types::DeleteParams& params)
    -> std::expected<Types::AssetOperationResult, std::string>;

// ============= 扫描和索引 =============

// 扫描资产目录并建立索引
auto scan_directories(Core::State::AppState& app_state, const Types::ScanParams& params)
    -> std::expected<Types::AssetScanResult, std::string>;

// 重新扫描特定目录（增量更新）
auto rescan_directory(Core::State::AppState& app_state, const std::string& directory_path,
                            bool generate_thumbnails = true)
    -> std::expected<Types::AssetScanResult, std::string>;

// ============= 缩略图管理 =============

// 获取缩略图数据
auto get_asset_thumbnail_data(Core::State::AppState& app_state, const Types::GetThumbnailParams& params)
    -> std::expected<std::vector<std::uint8_t>, std::string>;

// 批量生成缺失的缩略图
auto generate_missing_thumbnails(Core::State::AppState& app_state, std::uint32_t max_width = 400,
                                 std::uint32_t max_height = 400)
    -> std::expected<Types::AssetScanResult, std::string>;

// 清理孤立的缩略图文件
auto cleanup_thumbnails(Core::State::AppState& app_state)
    -> std::expected<Types::AssetOperationResult, std::string>;

// ============= 统计和信息 =============

// 获取资产库统计信息
auto get_asset_stats(Core::State::AppState& app_state, const Types::GetStatsParams& params)
    -> std::expected<Types::AssetStats, std::string>;

// 获取缩略图统计信息
auto get_thumbnail_stats(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string>;  // 返回格式化的统计信息字符串

// ============= 维护和优化 =============

// 数据库维护：清理软删除的记录
auto cleanup_deleted_assets(Core::State::AppState& app_state, int days_old = 30)
    -> std::expected<Types::AssetOperationResult, std::string>;

// 验证资产文件完整性
auto verify_asset_integrity(Core::State::AppState& app_state)
    -> std::expected<Types::AssetScanResult, std::string>;

// 重建缩略图索引
auto rebuild_asset_thumbnail_index(Core::State::AppState& app_state)
    -> std::expected<Types::AssetScanResult, std::string>;

// ============= 配置管理 =============

// 获取默认扫描选项
auto get_default_asset_scan_options() -> Types::AssetScanOptions;

// 验证扫描参数
auto validate_asset_scan_params(const Types::ScanParams& params) -> std::expected<void, std::string>;

}  // namespace Features::Asset
