#include "connection_manager.hpp"
#include <iostream>
#include "media/utils/logger.hpp"

ConnectionManager::ConnectionManager() : m_running(false) {}

ConnectionManager::~ConnectionManager() {
    stop();
}

void ConnectionManager::start() {
    if (m_running) return;
    
    m_running = true;
    m_cleanupThread = std::thread(&ConnectionManager::cleanupLoop, this);
}

void ConnectionManager::stop() {
    if (!m_running) return;
    
    m_running = false;
    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }
}

void ConnectionManager::addConnection(uint16_t local_port, uint32_t remote_addr, uint16_t remote_port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& conn = m_connections[local_port];
    conn.remote_addr = remote_addr;
    conn.remote_port = remote_port;
    conn.last_activity = std::chrono::steady_clock::now();
    spdlog::debug("ConnectionManager: Added new connection for port {}", local_port);
}

void ConnectionManager::updateConnection(uint16_t local_port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(local_port);
    if (it != m_connections.end()) {
        it->second.last_activity = std::chrono::steady_clock::now();
    }
}

void ConnectionManager::removeConnection(uint16_t local_port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(local_port);
}

bool ConnectionManager::hasConnection(uint16_t local_port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connections.find(local_port) != m_connections.end();
}

ConnectionInfo* ConnectionManager::getConnection(uint16_t local_port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(local_port);
    return (it != m_connections.end()) ? &(it->second) : nullptr;
}

void ConnectionManager::cleanupLoop() {
    while (m_running) {
        // 休眠指定的清理间隔时间
        std::this_thread::sleep_for(
            std::chrono::milliseconds(CONNECTION_CLEANUP_INTERVAL));
        
        // 获取锁并清理过期连接
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 获取当前时间
        auto now = std::chrono::steady_clock::now();
        
        // 遍历并清理过期连接
        for (auto it = m_connections.begin(); it != m_connections.end();) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.last_activity);
                
            if (duration.count() > CONNECTION_TIMEOUT) {
                spdlog::debug("ConnectionManager: Removing stale connection for port {}", 
                            it->first);
                it = m_connections.erase(it);
            } else {
                ++it;
            }
        }
    }
}