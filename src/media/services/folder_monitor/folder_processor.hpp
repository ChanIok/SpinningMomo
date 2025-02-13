#pragma once

#include <mutex>
#include <queue>
#include <vector>
#include <filesystem>
#include <condition_variable>
#include "processing_types.hpp"
#include "core/thread_raii.hpp"
#include "media/repositories/screenshot_repository.hpp"
#include "media/services/thumbnail_service.hpp"

class FolderProcessor {
public:
    static FolderProcessor& get_instance();
    
    // 处理单个文件夹
    bool process_folder(const std::string& folder_path);
    // 处理多个文件夹
    void process_folders(const std::vector<std::string>& folder_paths);
    // 处理单个文件
    bool process_single_file(const std::filesystem::path& file_path);
    
    // 等待所有任务完成
    void wait();
    // 停止处理
    void stop();

    // 获取处理进度
    ProcessingProgress get_progress(const std::string& folder_path);

private:
    FolderProcessor();
    ~FolderProcessor();

    // 禁止拷贝和移动
    FolderProcessor(const FolderProcessor&) = delete;
    FolderProcessor& operator=(const FolderProcessor&) = delete;
    FolderProcessor(FolderProcessor&&) = delete;
    FolderProcessor& operator=(FolderProcessor&&) = delete;

    // 线程池相关
    static constexpr size_t MAX_THREADS = 4;
    static constexpr size_t QUEUE_SIZE_LIMIT = 1000;
    static constexpr size_t BATCH_SIZE = 50;

    bool m_running{false};
    std::mutex m_mutex;
    std::condition_variable m_not_empty;
    std::condition_variable m_not_full;
    std::queue<std::filesystem::path> m_tasks;
    std::vector<ThreadRAII> m_workers;

    // 进度跟踪
    std::mutex m_progress_mutex;
    std::unordered_map<std::string, ProcessingProgress> m_progress;

    // 当前处理的文件夹信息
    int m_current_folder_id{0};
    std::string m_base_path;

    // 工作线程函数
    void worker_thread();
    // 处理一批文件
    void process_batch(const std::vector<std::filesystem::path>& batch);
    // 扫描文件夹
    void scan_folder(const std::filesystem::path& folder_path);
    
    // 创建截图对象
    Screenshot create_screenshot_from_file(const std::filesystem::path& file_path);
    // 获取文件时间信息
    struct FileTimeInfo {
        int64_t creation_time;
        int64_t last_write_time;
    };
    FileTimeInfo get_file_times(const std::filesystem::path& file_path);
    // 从文件名解析照片时间
    std::optional<int64_t> parse_photo_time_from_filename(const std::string& filename);
    // 验证文件是否为支持的图片格式
    bool is_supported_image(const std::filesystem::path& file_path);
    // 更新进度
    void update_progress(const std::string& folder_path, const ProcessingProgress& progress);
    // 计算相对路径
    std::string calculate_relative_path(const std::string& filepath, const std::string& base_path);

    // 服务依赖
    ScreenshotRepository& m_screenshot_repository = ScreenshotRepository::get_instance();
    ThumbnailService& m_thumbnail_service = ThumbnailService::get_instance();
}; 