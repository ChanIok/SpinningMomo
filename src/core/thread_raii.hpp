#pragma once
#include <thread>
#include <memory>
#include "win_config.hpp"

class ThreadRAII {
public:
    // 默认构造函数
    ThreadRAII() = default;

    // 构造函数：接受任何可调用对象和其参数
    template<typename Fn, typename... Args>
    explicit ThreadRAII(Fn&& fn, Args&&... args)
        : thread_(std::make_unique<std::thread>(
            std::forward<Fn>(fn), 
            std::forward<Args>(args)...)) {}

    // 析构函数：确保线程被正确join
    ~ThreadRAII() noexcept {
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
    }
    
    // 禁用拷贝
    ThreadRAII(const ThreadRAII&) = delete;
    ThreadRAII& operator=(const ThreadRAII&) = delete;
    
    // 允许移动
    ThreadRAII(ThreadRAII&&) noexcept = default;
    ThreadRAII& operator=(ThreadRAII&&) noexcept = default;

    // 赋值操作符，用于延迟初始化
    template<typename Fn, typename... Args>
    ThreadRAII& operator=(std::unique_ptr<std::thread>&& thread) noexcept {
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
        thread_ = std::move(thread);
        return *this;
    }

    // 访问底层线程对象
    std::thread* get() noexcept { 
        return thread_.get(); 
    }
    
    const std::thread* get() const noexcept { 
        return thread_.get(); 
    }

    // 获取线程ID
    std::thread::id get_id() const noexcept { 
        return thread_ ? thread_->get_id() : std::thread::id();
    }

private:
    std::unique_ptr<std::thread> thread_;
}; 