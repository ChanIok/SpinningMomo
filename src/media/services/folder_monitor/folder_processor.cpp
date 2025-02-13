#include "folder_processor.hpp"
#include <spdlog/spdlog.h>
#include "media/utils/string_utils.hpp"
#include "core/settings/settings_manager.hpp"
#include "media/utils/time_utils.hpp"
#include "core/image_processor.hpp"

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

// 处理文件夹
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

// 处理多个文件夹
void FolderProcessor::process_folders(const std::vector<std::string>& folder_paths) {
    for (const auto& path : folder_paths) {
        process_folder(path);
    }
}

// 获取进度
ProcessingProgress FolderProcessor::get_progress(const std::string& folder_path) {
    std::lock_guard<std::mutex> lock(m_progress_mutex);
    return m_progress[folder_path];
}

// 工作线程
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

// 处理单个文件
bool FolderProcessor::process_single_file(const std::filesystem::path& file_path) {
    try {
        std::string utf8_path = wide_to_utf8(file_path.wstring());
        
        // 检查文件是否已在数据库中
        if (m_screenshot_repository.exists_by_path(utf8_path)) {
            return true;
        }

        // 创建截图对象
        auto screenshot = create_screenshot_from_file(file_path);
        // 输出screenshot.folder_id和screenshot.relative_path
        spdlog::info("Folder ID: {}", screenshot.folder_id);
        spdlog::info("Relative Path: {}", screenshot.relative_path);
        // 保存到数据库
        if (!m_screenshot_repository.save(screenshot)) {
            throw std::runtime_error("Failed to save screenshot");
        }

        // 生成缩略图
        return m_thumbnail_service.generate_thumbnail(screenshot);
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing file {}: {}", 
            wide_to_utf8(file_path.wstring()), e.what());
        return false;
    }
}

// 处理一批文件
void FolderProcessor::process_batch(const std::vector<std::filesystem::path>& batch) {
    for (const auto& file_path : batch) {
        if (!process_single_file(file_path)) {
            spdlog::error("Failed to process file: {}", wide_to_utf8(file_path.wstring()));
        }
    }
}

// 扫描文件夹
void FolderProcessor::scan_folder(const std::filesystem::path& folder_path) {
    try {
        std::string utf8_folder_path = wide_to_utf8(folder_path.wstring());
        ProcessingProgress progress;
        progress.status = ProcessingStatus::Processing;
        
        // 设置当前处理的文件夹信息
        auto& settings = SettingsManager::get_instance();
        for (const auto& folder : settings.get_watched_folders()) {
            if (folder.path == utf8_folder_path) {
                m_current_folder_id = folder.id;
                m_base_path = folder.path;
                break;
            }
        }
        
        if (m_current_folder_id == 0) {
            throw std::runtime_error("Folder not found in watched folders");
        }
        
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

// 检查文件是否支持
bool FolderProcessor::is_supported_image(const std::filesystem::path& file_path) {
    auto ext = file_path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == L".jpg" || ext == L".jpeg" || ext == L".png";
}

// 更新进度
void FolderProcessor::update_progress(const std::string& folder_path, const ProcessingProgress& progress) {
    std::lock_guard<std::mutex> lock(m_progress_mutex);
    m_progress[folder_path] = progress;
}

Screenshot FolderProcessor::create_screenshot_from_file(const std::filesystem::path& file_path) {
    Screenshot screenshot;
    
    // 1. 设置基本文件信息
    screenshot.filepath = wide_to_utf8(file_path.wstring());
    screenshot.filename = wide_to_utf8(file_path.filename().wstring());
    screenshot.file_size = std::filesystem::file_size(file_path);
    
    // 2. 设置文件夹信息
    screenshot.folder_id = m_current_folder_id;
    screenshot.relative_path = calculate_relative_path(
        screenshot.filepath, m_base_path);

    // 3. 读取文件时间
    auto file_times = get_file_times(file_path);
    screenshot.created_at = file_times.creation_time;
    screenshot.updated_at = TimeUtils::now();
    screenshot.photo_time = parse_photo_time_from_filename(
        file_path.filename().string());
    
    // 4. 读取图片信息
    auto source = ImageProcessor::LoadFromFile(file_path);
    if (!source) {
        throw std::runtime_error("Failed to load image");
    }
    ImageProcessor::GetImageDimensions(
        source.Get(), 
        screenshot.width, 
        screenshot.height
    );
    
    // 5. 设置其他默认值
    screenshot.metadata = "{}";
    screenshot.thumbnail_generated = false;
    
    return screenshot;
}

FolderProcessor::FileTimeInfo FolderProcessor::get_file_times(const std::filesystem::path& file_path) {
    HANDLE hFile = CreateFileW(
        file_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open file: " + wide_to_utf8(file_path.wstring()));
    }
    
    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (!GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime)) {
        CloseHandle(hFile);
        throw std::runtime_error("Failed to get file times");
    }
    CloseHandle(hFile);
    
    // 转换时间
    const uint64_t EPOCH_DIFFERENCE = 116444736000000000ULL;
    
    ULARGE_INTEGER uli;
    uli.LowPart = creationTime.dwLowDateTime;
    uli.HighPart = creationTime.dwHighDateTime;
    int64_t creation_time = (uli.QuadPart - EPOCH_DIFFERENCE) / 10000000ULL;
    
    uli.LowPart = lastWriteTime.dwLowDateTime;
    uli.HighPart = lastWriteTime.dwHighDateTime;
    int64_t last_write_time = (uli.QuadPart - EPOCH_DIFFERENCE) / 10000000ULL;
    
    return {creation_time, last_write_time};
}

std::optional<int64_t> FolderProcessor::parse_photo_time_from_filename(const std::string& filename) {
    // 分割文件名 2024_12_05_23_50_29_6626579.jpeg
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = filename.find('_');
    
    while (end != std::string::npos) {
        parts.push_back(filename.substr(start, end - start));
        start = end + 1;
        end = filename.find('_', start);
    }
    
    // 如果格式不正确，返回空
    if (parts.size() < 6) {
        return std::nullopt;
    }
    
    try {
        // 解析时间组件
        struct tm timeinfo = {};
        timeinfo.tm_year = std::stoi(parts[0]) - 1900;  // 年份需要减去1900
        timeinfo.tm_mon = std::stoi(parts[1]) - 1;      // 月份从0开始
        timeinfo.tm_mday = std::stoi(parts[2]);
        timeinfo.tm_hour = std::stoi(parts[3]);
        timeinfo.tm_min = std::stoi(parts[4]);
        timeinfo.tm_sec = std::stoi(parts[5]);
        
        // 转换为时间戳
        return std::mktime(&timeinfo);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string FolderProcessor::calculate_relative_path(const std::string& filepath, const std::string& base_path) {
    std::filesystem::path path(filepath);
    std::filesystem::path base(base_path);
    
    // 如果是文件路径，获取其父目录
    if (std::filesystem::is_regular_file(filepath)) {
        path = path.parent_path();
    }
    
    // 计算相对路径
    auto rel_path = std::filesystem::relative(path, base);
    return rel_path.string();
} 