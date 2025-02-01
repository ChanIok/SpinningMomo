#pragma once

#include "win_config.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <vector>
#include <future>
#include "media/db/models.hpp"
#include "core/image_processor.hpp"
#include "core/thread_raii.hpp"

class ThumbnailBatchProcessor {
public:
    static ThumbnailBatchProcessor& get_instance();
    
    // 添加任务到队列
    void add_task(const Screenshot& screenshot);
    
    // 批量添加任务
    void add_tasks(const std::vector<Screenshot>& screenshots);
    
    // 启动处理
    void start();
    
    // 停止处理
    void stop();
    
    // 等待当前任务完成
    void wait();

private:
    ThumbnailBatchProcessor();
    ~ThumbnailBatchProcessor();
    
    // 工作线程函数
    void worker_thread();
    
    // 处理单个缩略图
    bool process_single(const Screenshot& screenshot);
    
    // 处理一批缩略图
    void process_batch(const std::vector<Screenshot>& batch);
    
    // 线程池
    std::vector<ThreadRAII> m_workers;
    
    // 任务队列
    std::queue<Screenshot> m_tasks;
    std::mutex m_queue_mutex;
    std::condition_variable m_condition;
    
    // 控制标志
    std::atomic<bool> m_running{false};
    
    // 配置参数
    static constexpr size_t MAX_THREADS = 4;        // 最大线程数
    static constexpr size_t BATCH_SIZE = 5;         // 每批处理数量
    static constexpr size_t QUEUE_SIZE_LIMIT = 50;  // 队列大小限制
}; 