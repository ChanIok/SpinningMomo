#include "thumbnail_batch_processor.hpp"
#include "thumbnail_service.hpp"
#include <spdlog/spdlog.h>
#include "media/utils/string_utils.hpp"

// 全局静态指针
static ThumbnailBatchProcessor* g_instance = nullptr;

ThumbnailBatchProcessor& ThumbnailBatchProcessor::get_instance() {
    if (!g_instance) {
        g_instance = new ThumbnailBatchProcessor();
    }
    return *g_instance;
}

ThumbnailBatchProcessor::ThumbnailBatchProcessor() {
    start();
}

ThumbnailBatchProcessor::~ThumbnailBatchProcessor() {
    stop();
}

void ThumbnailBatchProcessor::start() {
    if (m_running) return;
    
    m_running = true;
    
    // 创建工作线程
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        m_workers.emplace_back(&ThumbnailBatchProcessor::worker_thread, this);
    }
}

void ThumbnailBatchProcessor::stop() {
    m_running = false;
    // 通知所有等待的线程
    m_not_empty.notify_all();
    m_not_full.notify_all();
    
    // 清空线程池 - ThreadRAII 会自动处理线程的 join
    m_workers.clear();
}

void ThumbnailBatchProcessor::wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_not_empty.wait(lock, [this]() { return m_tasks.empty(); });
}

void ThumbnailBatchProcessor::add_task(const Screenshot& screenshot) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    // 等待队列有空间
    m_not_full.wait(lock, [this]() { 
        return m_tasks.size() < QUEUE_SIZE_LIMIT || !m_running; 
    });
    
    if (!m_running) return;
    
    m_tasks.push(screenshot);
    // 通知一个工作线程
    m_not_empty.notify_one();
}

void ThumbnailBatchProcessor::add_tasks(const std::vector<Screenshot>& screenshots) {
    if (!m_running) {
        start();  // 确保处理器在运行
    }
    
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t added = 0;
    
    for (const auto& screenshot : screenshots) {
        // 等待队列有空间
        while (m_tasks.size() >= QUEUE_SIZE_LIMIT && m_running) {
            spdlog::info("Queue full, waiting for space ({} tasks remaining)...", screenshots.size() - added);
            m_not_full.wait(lock);
        }
        
        if (!m_running) break;
        
        m_tasks.push(screenshot);
        added++;
        
        if (added % 10 == 0) {  // 每添加10个任务通知一次，因为有多个工作线程可以并行处理
            m_not_empty.notify_all();
        }
    }
    
    if (added > 0) {
        spdlog::info("Added {} tasks to thumbnail generation queue", added);
        // 批量添加完成后通知所有工作线程
        m_not_empty.notify_all();
    }
}

void ThumbnailBatchProcessor::worker_thread() {
    std::vector<Screenshot> batch;
    batch.reserve(BATCH_SIZE);
    
    while (m_running) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            // 等待任务或停止信号
            m_not_empty.wait(lock, [this]() {
                return !m_tasks.empty() || !m_running;
            });
            
            if (!m_running && m_tasks.empty()) break;
            
            // 收集一批任务
            while (!m_tasks.empty() && batch.size() < BATCH_SIZE) {
                batch.push_back(m_tasks.front());
                m_tasks.pop();
                // 通知一个生产者线程
                m_not_full.notify_one();
            }
        }
        
        if (!batch.empty()) {
            process_batch(batch);
            batch.clear();
        }
    }
}

bool ThumbnailBatchProcessor::process_single(const Screenshot& screenshot) {
    try {
        Screenshot mutable_screenshot = screenshot;  // 创建可修改的副本
        // 将UTF-8路径转换为宽字符串
        std::wstring wide_path = utf8_to_wide(screenshot.filepath);
        
        // 加载原图
        auto source = ImageProcessor::LoadFromFile(wide_path);
        if (!source) {
            return false;
        }
        
        // 获取原图尺寸
        UINT width = 0, height = 0;
        ImageProcessor::GetImageDimensions(source.Get(), width, height);
        
        // 计算缩放比例，目标高度为480px
        double scale = 480.0 / height;
        UINT targetWidth = static_cast<UINT>(width * scale);
        
        // 调整大小
        auto resized = ImageProcessor::Resize(
            source.Get(),
            targetWidth,
            480,
            WICBitmapInterpolationModeHighQualityCubic
        );
        if (!resized) {
            return false;
        }
        
        // 获取缩略图路径
        auto& thumbnail_service = ThumbnailService::get_instance();
        auto thumbnail_path = thumbnail_service.get_thumbnail_path(screenshot);
        
        // 确保缩略图目录存在
        thumbnail_service.ensure_thumbnail_dir();
        
        // 保存为JPEG
        if (!ImageProcessor::SaveToJpegFile(resized.Get(), thumbnail_path.wstring(), 0.85f)) {
            return false;
        }
        
        // 更新数据库状态
        return repository_.update_thumbnail_generated(screenshot.id, true);
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing thumbnail for {}: {}", screenshot.filename, e.what());
        return false;
    }
}

void ThumbnailBatchProcessor::process_batch(const std::vector<Screenshot>& batch) {
    for (const auto& screenshot : batch) {
        if (!process_single(screenshot)) {
            spdlog::error("Failed to process thumbnail for: {}", screenshot.filename);
        }
    }
} 