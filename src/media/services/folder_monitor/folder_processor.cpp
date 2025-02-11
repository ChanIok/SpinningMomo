#include "folder_processor.hpp"
#include <spdlog/spdlog.h>
#include "media/utils/string_utils.hpp"

// 全局静态指针
static FolderProcessor* g_instance = nullptr;

FolderProcessor& FolderProcessor::get_instance() {
    if (!g_instance) {
        g_instance = new FolderProcessor();
    }
    return *g_instance;
}

FolderProcessor::FolderProcessor() {
    m_running = true;
    // 创建工作线程
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        m_workers.emplace_back(&FolderProcessor::worker_thread, this);
    }
}

FolderProcessor::~FolderProcessor() {
    stop();
}

void FolderProcessor::stop() {
    if (!m_running) return;
    
    m_running = false;
    m_not_empty.notify_all();
    m_not_full.notify_all();
    
    // 清空线程池
    m_workers.clear();
}

void FolderProcessor::wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_not_empty.wait(lock, [this]() { return m_tasks.empty(); });
}

bool FolderProcessor::process_folder(const std::string& folder_path) {
    try {
        std::filesystem::path fs_path = utf8_to_wide(folder_path);
        if (!std::filesystem::exists(fs_path)) {
            spdlog::error("Folder does not exist: {}", folder_path);
            return false;
        }

        // 初始化进度信息
        ProcessingProgress progress;
        progress.status = ProcessingStatus::Processing;
        update_progress(folder_path, progress);

        // 开始扫描文件夹
        scan_folder(fs_path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error processing folder {}: {}", folder_path, e.what());
        return false;
    }
}

void FolderProcessor::process_folders(const std::vector<std::string>& folder_paths) {
    for (const auto& path : folder_paths) {
        process_folder(path);
    }
}

ProcessingProgress FolderProcessor::get_progress(const std::string& folder_path) {
    std::lock_guard<std::mutex> lock(m_progress_mutex);
    return m_progress[folder_path];
}

void FolderProcessor::worker_thread() {
    std::vector<std::filesystem::path> batch;
    batch.reserve(BATCH_SIZE);
    
    while (m_running) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            m_not_empty.wait(lock, [this]() {
                return !m_tasks.empty() || !m_running;
            });
            
            if (!m_running && m_tasks.empty()) break;
            
            while (!m_tasks.empty() && batch.size() < BATCH_SIZE) {
                batch.push_back(m_tasks.front());
                m_tasks.pop();
                m_not_full.notify_one();
            }
        }
        
        if (!batch.empty()) {
            process_batch(batch);
            batch.clear();
        }
    }
}

bool FolderProcessor::process_single_file(const std::filesystem::path& file_path) {
    try {
        std::string utf8_path = wide_to_utf8(file_path.wstring());
        
        // 检查文件是否已在数据库中
        if (m_screenshot_repository.exists_by_path(utf8_path)) {
            return true;
        }

        // 创建Screenshot对象
        Screenshot screenshot;
        screenshot.filepath = utf8_path;
        screenshot.filename = wide_to_utf8(file_path.filename().wstring());
        
        // 保存到数据库
        if (!m_screenshot_repository.save(screenshot)) {
            spdlog::error("Failed to save screenshot to database: {}", screenshot.filename);
            return false;
        }

        // 生成缩略图
        return m_thumbnail_service.generate_thumbnail(screenshot);
    } catch (const std::exception& e) {
        spdlog::error("Error processing file {}: {}", wide_to_utf8(file_path.wstring()), e.what());
        return false;
    }
}

void FolderProcessor::process_batch(const std::vector<std::filesystem::path>& batch) {
    for (const auto& file_path : batch) {
        if (!process_single_file(file_path)) {
            spdlog::error("Failed to process file: {}", wide_to_utf8(file_path.wstring()));
        }
    }
}

void FolderProcessor::scan_folder(const std::filesystem::path& folder_path) {
    try {
        std::string utf8_folder_path = wide_to_utf8(folder_path.wstring());
        ProcessingProgress progress;
        progress.status = ProcessingStatus::Processing;
        
        // 计算总文件数
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folder_path)) {
            if (entry.is_regular_file() && is_supported_image(entry.path())) {
                progress.total_files++;
            }
        }
        
        update_progress(utf8_folder_path, progress);
        
        // 添加文件到处理队列
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folder_path)) {
            if (!m_running) break;
            
            if (entry.is_regular_file() && is_supported_image(entry.path())) {
                std::unique_lock<std::mutex> lock(m_mutex);
                
                m_not_full.wait(lock, [this]() { 
                    return m_tasks.size() < QUEUE_SIZE_LIMIT || !m_running; 
                });
                
                if (!m_running) break;
                
                m_tasks.push(entry.path());
                progress.current_file = wide_to_utf8(entry.path().filename().wstring());
                progress.processed_files++;
                update_progress(utf8_folder_path, progress);
                
                m_not_empty.notify_one();
            }
        }
        
        // 更新完成状态
        progress.status = ProcessingStatus::Completed;
        update_progress(utf8_folder_path, progress);
        
    } catch (const std::exception& e) {
        spdlog::error("Error scanning folder {}: {}", wide_to_utf8(folder_path.wstring()), e.what());
        ProcessingProgress progress;
        progress.status = ProcessingStatus::Failed;
        progress.error_message = e.what();
        update_progress(wide_to_utf8(folder_path.wstring()), progress);
    }
}

bool FolderProcessor::is_supported_image(const std::filesystem::path& file_path) {
    auto ext = file_path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == L".jpg" || ext == L".jpeg" || ext == L".png";
}

void FolderProcessor::update_progress(const std::string& folder_path, const ProcessingProgress& progress) {
    std::lock_guard<std::mutex> lock(m_progress_mutex);
    m_progress[folder_path] = progress;
} 