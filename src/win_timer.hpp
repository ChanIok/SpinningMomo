#pragma once
#include "win_config.hpp"
#include <functional>
#include <atomic>
#include <thread>

class WinTimer {
public:
    // 定时器状态
    enum class State {
        Idle,       // 空闲状态，可以设置定时器
        Running,    // 定时器正在运行
        Triggered   // 定时器已触发
    };

    WinTimer();
    ~WinTimer();

    // 禁用拷贝
    WinTimer(const WinTimer&) = delete;
    WinTimer& operator=(const WinTimer&) = delete;

    // 设置定时器，只有在 Idle 或 Triggered 状态时才能调用
    // 返回是否成功设置
    bool SetTimer(DWORD delayMs, std::function<void()> callback);
    
    // 重置定时器时间，保持相同的回调
    bool Reset();
    
    // 取消定时器
    void Cancel();

    // 获取定时器状态
    State GetState() const { return m_state; }
    
    // 检查定时器是否正在运行
    bool IsRunning() const { return m_state == State::Running; }

private:
    HANDLE m_timer = nullptr;
    std::atomic<State> m_state{State::Idle};
    std::function<void()> m_callback;
    DWORD m_delayMs = 0;
    std::thread m_waitThread;

    // 创建定时器对象
    bool CreateTimerObject();
    
    // 启动等待线程
    void StartWaitThread();
};