module;

export module Features.Gallery.Scanner;

import std;
import Core.State;
import Features.Gallery.Types;
import Utils.Image;

namespace Features::Gallery::Scanner {

export auto scan_asset_directory(Core::State::AppState& app_state,
                                 const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string>;

// 从文件路径提取资产信息
export auto extract_asset_info(Utils::Image::WICFactory& wic_factory,
                               const std::filesystem::path& file_path)
    -> std::expected<Types::Info, std::string>;

// 检测资产类型（基于文件扩展名和内容）
export auto detect_asset_type(const std::filesystem::path& file_path) -> std::string;

// 检查文件是否为支持的资产格式
export auto is_supported_file(const std::filesystem::path& file_path,
                              const std::vector<std::string>& supported_extensions) -> bool;

// 验证文件是否可访问且可读
export auto is_file_accessible(const std::filesystem::path& file_path) -> bool;

// 计算文件哈希
export auto calculate_file_hash(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string>;

// 扫描目录中的文件信息（支持忽略规则）
export auto scan_file_info(Core::State::AppState& app_state, const std::filesystem::path& directory,
                           const Types::ScanOptions& options,
                           std::optional<std::int64_t> folder_id = std::nullopt)
    -> std::expected<std::vector<Types::FileSystemInfo>, std::string>;

// 分析文件变化
export auto analyze_file_changes(
    const std::vector<Types::FileSystemInfo>& file_infos,
    const std::unordered_map<std::string, Types::Metadata>& asset_cache)
    -> std::vector<Types::FileAnalysisResult>;

// 并行处理文件
export auto process_files_in_parallel(
    Core::State::AppState& app_state,
    const std::vector<Types::FileAnalysisResult>& files_to_process,
    const Types::ScanOptions& options) -> std::expected<Types::ProcessingBatchResult, std::string>;

// 优化的单文件处理
export auto process_single_file_optimized(Core::State::AppState& app_state,
                                          Utils::Image::WICFactory& wic_factory,
                                          const Types::FileAnalysisResult& analysis,
                                          const Types::ScanOptions& options)
    -> std::expected<Types::Asset, std::string>;

}  // namespace Features::Gallery::Scanner
