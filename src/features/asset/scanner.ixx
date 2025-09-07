module;

export module Features.Asset.Scanner;

import std;
import Core.State;
import Features.Asset.Types;
import Utils.Image;

export namespace Features::Asset::Scanner {

// ============= 文件扫描功能 =============

// 扫描指定目录中的资产文件
auto scan_asset_directory(Core::State::AppState& app_state, 
                   const Types::AssetScanOptions& options)
    -> std::expected<Types::AssetScanResult, std::string>;

// 递归扫描目录，返回符合条件的文件路径列表
auto find_files(const std::filesystem::path& directory,
                     const std::vector<std::string>& supported_extensions,
                     bool recursive = true)
    -> std::expected<std::vector<std::filesystem::path>, std::string>;

// ============= 资产信息提取功能 =============

// 从文件路径提取资产信息
auto extract_asset_info(Utils::Image::WICFactory& wic_factory,
                       const std::filesystem::path& file_path)
    -> std::expected<Types::AssetInfo, std::string>;

// 检测资产类型（基于文件扩展名和内容）
auto detect_asset_type(const std::filesystem::path& file_path)
    -> std::string;

// 根据文件路径计算相对路径
auto calculate_asset_relative_path(const std::filesystem::path& file_path,
                           const std::filesystem::path& base_directory)
    -> std::string;

// ============= 文件验证功能 =============

// 检查文件是否为支持的资产格式
auto is_supported_file(const std::filesystem::path& file_path,
                            const std::vector<std::string>& supported_extensions)
    -> bool;

// 验证文件是否可访问且可读
auto is_file_accessible(const std::filesystem::path& file_path)
    -> bool;

// ============= 批量处理功能 =============

// 批量处理文件列表，提取资产信息
auto process_files(Core::State::AppState& app_state,
                        Utils::Image::WICFactory& wic_factory,
                        const std::vector<std::filesystem::path>& file_paths,
                        const std::filesystem::path& base_directory,
                        bool generate_thumbnails = true)
    -> std::expected<std::vector<Types::Asset>, std::string>;

// 处理单个资产文件，创建 Asset
auto process_single_file(Core::State::AppState& app_state,
                              Utils::Image::WICFactory& wic_factory,
                              const std::filesystem::path& file_path,
                              const std::filesystem::path& base_directory,
                              bool generate_thumbnail = true)
    -> std::expected<Types::Asset, std::string>;

// ============= 增量扫描功能 =============

// 检查文件是否已在数据库中存在
auto is_file_already_indexed(Core::State::AppState& app_state,
                            const std::filesystem::path& file_path)
    -> std::expected<bool, std::string>;

// 检查已索引文件是否需要更新（基于修改时间）
auto needs_update(Core::State::AppState& app_state,
                 const std::filesystem::path& file_path)
    -> std::expected<bool, std::string>;

// ============= 错误处理和统计 =============

// 扫描过程中的错误信息
struct AssetScanError {
    std::filesystem::path file_path;
    std::string error_message;
    std::string error_type;  // "access_denied", "unsupported_format", "processing_error"
};

// 扫描统计信息
struct AssetScanStatistics {
    int total_files_found = 0;
    int files_processed = 0;
    int files_skipped = 0;
    int new_files_added = 0;
    int files_updated = 0;
    int errors_count = 0;
    std::vector<AssetScanError> errors;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    
    auto get_duration_string() const -> std::string;
};

// 将 AssetScanStatistics 转换为 AssetScanResult
auto asset_statistics_to_result(const AssetScanStatistics& stats) -> Types::AssetScanResult;

} // namespace Features::Asset::Scanner
