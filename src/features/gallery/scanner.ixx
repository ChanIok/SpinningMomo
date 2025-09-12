module;

export module Features.Gallery.Scanner;

import std;
import Core.State;
import Features.Gallery.Types;
import Utils.Image;

export namespace Features::Gallery::Scanner {

// ============= 文件扫描功能 =============

// 扫描指定目录中的资产文件
auto scan_asset_directory(Core::State::AppState& app_state, const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string>;

// 递归扫描目录，返回符合条件的文件路径列表
auto find_files(const std::filesystem::path& directory,
                const std::vector<std::string>& supported_extensions, bool recursive = true)
    -> std::expected<std::vector<std::filesystem::path>, std::string>;

// ============= 资产信息提取功能 =============

// 从文件路径提取资产信息
auto extract_asset_info(Utils::Image::WICFactory& wic_factory,
                        const std::filesystem::path& file_path)
    -> std::expected<Types::Info, std::string>;

// 检测资产类型（基于文件扩展名和内容）
auto detect_asset_type(const std::filesystem::path& file_path) -> std::string;

// 根据文件路径计算相对路径
auto calculate_asset_relative_path(const std::filesystem::path& file_path,
                                   const std::filesystem::path& base_directory) -> std::string;

// ============= 文件验证功能 =============

// 检查文件是否为支持的资产格式
auto is_supported_file(const std::filesystem::path& file_path,
                       const std::vector<std::string>& supported_extensions) -> bool;

// 验证文件是否可访问且可读
auto is_file_accessible(const std::filesystem::path& file_path) -> bool;

// ============= 批量处理功能 =============

// ============= 多线程优化函数 =============

// 计算文件哈希
auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string>;

// 并行发现文件
auto discover_files_parallel(Core::State::AppState& app_state,
                             const std::vector<std::filesystem::path>& directories,
                             const Types::ScanOptions& options)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string>;

// 分析文件变化
auto analyze_file_changes(const std::vector<Types::FileSystemInfo>& discovered_files,
                          const Types::Cache& asset_cache)
    -> std::vector<Types::FileAnalysisResult>;

// 并行处理文件
auto process_files_in_parallel(Core::State::AppState& app_state,
                               const std::vector<Types::FileAnalysisResult>& files_to_process,
                               const Types::ScanOptions& options)
    -> std::expected<Types::ProcessingBatchResult, std::string>;

// 优化的单文件处理
auto process_single_file_optimized(Core::State::AppState& app_state,
                                   Utils::Image::WICFactory& wic_factory,
                                   const Types::FileAnalysisResult& analysis,
                                   const Types::ScanOptions& options)
    -> std::expected<Types::Asset, std::string>;

}  // namespace Features::Gallery::Scanner
