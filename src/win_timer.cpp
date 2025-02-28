#include "win_timer.hpp"
#include "logger.hpp"

WinTimer::WinTimer() {
    CreateTimerObject();
}

WinTimer::~WinTimer() {
    Cancel();
    if (m_timer) {
        CloseHandle(m_timer);
        m_timer = nullptr;
    }
}

bool WinTimer::CreateTimerObject() {
    if (!m_timer) {
        m_timer = CreateWaitableTimer(nullptr, TRUE, nullptr);
        if (!m_timer) {
            LOG_ERROR("Failed to create waitable timer");
            return false;
        }
    }
    return true;
}

bool WinTimer::SetTimer(DWORD delayMs, std::function<void()> callback) {
    // 检查当前状态
    State expectedState = State::Idle;
    if (!m_state.compare_exchange_strong(expectedState, State::Running)) {
        // 如果已经在运行，不允许再次设置
        if (m_state == State::Running) {
            LOG_ERROR("Timer already running, use Reset() instead");
            return false;
        }
        
        // 如果是已触发状态，重置为空闲状态
        if (m_state == State::Triggered) {
            m_state.store(State::Idle);
        }
    }
    
    if (!callback) {
        LOG_ERROR("Invalid callback");
        m_state.store(State::Idle);
        return false;
    }

    // 确保定时器对象存在
    if (!CreateTimerObject()) {
        m_state.store(State::Idle);
        return false;
    }

    // 取消任何现有等待
    Cancel();

    // 保存参数
    m_callback = std::move(callback);
    m_delayMs = delayMs;
    LOG_DEBUG("Setting timer for " + std::to_string(delayMs) + " ms");

    // 重置并启动定时器
    return Reset();
}

bool WinTimer::Reset() {
    // 只有在有回调和定时器对象的情况下才能重置
    if (!m_timer || !m_callback) {
        LOG_ERROR("Timer not properly initialized");
        return false;
    }
    
    LOG_DEBUG("Resetting timer");
    
    // 确保状态为运行
    State expectedState = State::Idle;
    if (!m_state.compare_exchange_strong(expectedState, State::Running)) {
        // 如果已经触发，需要先重置
        if (m_state == State::Triggered) {
            m_state.store(State::Running);
        }
        // 如果已在运行，无需更改状态
    }
    
    // 设置定时器
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -static_cast<LONGLONG>(m_delayMs) * 10000LL; // 转换为100纳秒单位
    if (!SetWaitableTimer(m_timer, &dueTime, 0, nullptr, nullptr, FALSE)) {
        LOG_ERROR("Failed to set waitable timer");
        m_state.store(State::Idle);
        return false;
    }

    // 启动等待线程
    StartWaitThread();
    return true;
}

void WinTimer::StartWaitThread() {
    LOG_DEBUG("Starting wait thread");
    
    // 捕获必要的变量，避免访问类成员
    HANDLE timerHandle = m_timer;
    auto callback = m_callback;
    
    m_waitThread = std::thread([this, timerHandle, callback]() {
        LOG_DEBUG("Wait thread started");
        
        // 等待定时器信号
        DWORD result = WaitForSingleObject(timerHandle, INFINITE);
        
        // 只有在等待成功且定时器还在运行状态时才执行回调
        State expectedState = State::Running;
        if (result == WAIT_OBJECT_0 && m_state.compare_exchange_strong(expectedState, State::Triggered)) {
            LOG_DEBUG("Timer triggered, executing callback");
            callback();
        }
    });
    
    // 分离线程使其独立运行
    m_waitThread.detach();
}

void WinTimer::Cancel() {
    // 设置状态为空闲，不管之前是什么状态
    State previousState = m_state.exchange(State::Idle);
    
    if (previousState == State::Running && m_timer) {
        LOG_DEBUG("Cancelling timer");
        CancelWaitableTimer(m_timer);
    }
}