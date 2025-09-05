module;

export module Features.Media.Scanner;

import std;
import Core.State;
import Features.Media.Types;
import Utils.Image;

export namespace Features::Media::Scanner {

// ============= 文件扫描功能 =============

// 扫描指定目录中的媒体文件
auto scan_directory(Core::State::AppState& app_state, 
                   const Types::ScanOptions& options)
    -> std::expected<Types::ScanResult, std::string>;

// 递归扫描目录，返回符合条件的文件路径列表
auto find_media_files(const std::filesystem::path& directory,
                     const std::vector<std::string>& supported_extensions,
                     bool recursive = true)
    -> std::expected<std::vector<std::filesystem::path>, std::string>;

// ============= 媒体信息提取功能 =============

// 从文件路径提取媒体信息
auto extract_media_info(Utils::Image::WICFactory& wic_factory,
                       const std::filesystem::path& file_path)
    -> std::expected<Types::MediaInfo, std::string>;

// 检测媒体类型（基于文件扩展名和内容）
auto detect_media_type(const std::filesystem::path& file_path)
    -> std::string;

// 根据文件路径计算相对路径
auto calculate_relative_path(const std::filesystem::path& file_path,
                           const std::filesystem::path& base_directory)
    -> std::string;

// ============= 文件验证功能 =============

// 检查文件是否为支持的媒体格式
auto is_supported_media_file(const std::filesystem::path& file_path,
                            const std::vector<std::string>& supported_extensions)
    -> bool;

// 验证文件是否可访问且可读
auto is_file_accessible(const std::filesystem::path& file_path)
    -> bool;

// ============= 批量处理功能 =============

// 批量处理文件列表，提取媒体信息
auto process_media_files(Core::State::AppState& app_state,
                        Utils::Image::WICFactory& wic_factory,
                        const std::vector<std::filesystem::path>& file_paths,
                        const std::filesystem::path& base_directory,
                        bool generate_thumbnails = true)
    -> std::expected<std::vector<Types::MediaItem>, std::string>;

// 处理单个媒体文件，创建 MediaItem
auto process_single_media_file(Core::State::AppState& app_state,
                              Utils::Image::WICFactory& wic_factory,
                              const std::filesystem::path& file_path,
                              const std::filesystem::path& base_directory,
                              bool generate_thumbnail = true)
    -> std::expected<Types::MediaItem, std::string>;

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
struct ScanError {
    std::filesystem::path file_path;
    std::string error_message;
    std::string error_type;  // "access_denied", "unsupported_format", "processing_error"
};

// 扫描统计信息
struct ScanStatistics {
    int total_files_found = 0;
    int files_processed = 0;
    int files_skipped = 0;
    int new_files_added = 0;
    int files_updated = 0;
    int errors_count = 0;
    std::vector<ScanError> errors;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    
    auto get_duration_string() const -> std::string;
};

// 将 ScanStatistics 转换为 ScanResult
auto statistics_to_result(const ScanStatistics& stats) -> Types::ScanResult;

} // namespace Features::Media::Scanner
