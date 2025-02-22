#pragma once
#include <thread>
#include <memory>

class ThreadRAII {
public:
    // 默认构造函数，创建一个未初始化的线程对象
    ThreadRAII() = default;

    // 模板构造函数：接受可调用对象及其参数，创建并启动线程
    template<typename Fn, typename... Args>
    explicit ThreadRAII(Fn&& fn, Args&&... args)
        : thread_(std::make_unique<std::thread>(
            std::forward<Fn>(fn), 
            std::forward<Args>(args)...)) {}

    // 析构函数：确保线程在对象销毁时被join，避免资源泄漏
    ~ThreadRAII() noexcept {
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
    }

    // 禁用拷贝构造和赋值，防止线程所有权混乱
    ThreadRAII(const ThreadRAII&) = delete;
    ThreadRAII& operator=(const ThreadRAII&) = delete;

    // 支持移动构造和移动赋值，允许线程所有权转移
    ThreadRAII(ThreadRAII&&) noexcept = default;
    ThreadRAII& operator=(ThreadRAII&&) noexcept = default;

    // 赋值运算符：支持将现有的std::unique_ptr<std::thread>移动到对象中
    ThreadRAII& operator=(std::unique_ptr<std::thread>&& thread) noexcept {
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
        thread_ = std::move(thread);
        return *this;
    }

    // 获取底层std::thread指针（非常量版本）
    std::thread* get() noexcept { return thread_.get(); }
    
    // 获取底层std::thread指针（常量版本）
    const std::thread* get() const noexcept { return thread_.get(); }

    // 获取线程ID
    std::thread::id getId() const noexcept { 
        return thread_ ? thread_->get_id() : std::thread::id{};
    }

    // 分离线程，使其在后台运行，析构时不再join
    void detach() noexcept {
        if (thread_ && thread_->joinable()) {
            thread_->detach();
        }
    }

private:
    // 使用unique_ptr管理std::thread，确保资源自动释放
    std::unique_ptr<std::thread> thread_;
};