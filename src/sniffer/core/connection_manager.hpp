#pragma once
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include "sniffer/types/common_types.hpp"

class ConnectionManager {
public:
    static ConnectionManager& getInstance() {
        static ConnectionManager instance;
        return instance;
    }

    // 连接管理
    void addConnection(uint16_t local_port, uint32_t remote_addr, uint16_t remote_port);
    void removeConnection(uint16_t local_port);
    void updateConnection(uint16_t local_port);
    
    // 查询接口
    bool hasConnection(uint16_t local_port);
    ConnectionInfo* getConnection(uint16_t local_port);
    
    // 启动/停止
    void start();
    void stop();

private:
    ConnectionManager();
    ~ConnectionManager();
    
    // 禁用拷贝和赋值
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 清理过期连接
    void cleanupLoop();

private:
    std::unordered_map<uint16_t, ConnectionInfo> m_connections;
    std::mutex m_mutex;
    std::atomic<bool> m_running;
    std::thread m_cleanupThread;
}; 