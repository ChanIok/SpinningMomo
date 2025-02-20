#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include "windivert.h"
#include "sniffer/types/common_types.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <chrono>
#include "process_manager.hpp"

// OpenSSL环境管理
class OpenSSLEnvironment {
public:
    static OpenSSLEnvironment& getInstance() {
        static OpenSSLEnvironment instance;
        return instance;
    }

    OpenSSLEnvironment(const OpenSSLEnvironment&) = delete;
    OpenSSLEnvironment& operator=(const OpenSSLEnvironment&) = delete;

private:
    OpenSSLEnvironment() {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    }

    ~OpenSSLEnvironment() {
        EVP_cleanup();
        ERR_free_strings();
    }
};

class ProxyServer;  // 前向声明

// 新增：临时包存储结构
struct PendingPacket {
    std::vector<uint8_t> data;  // 包数据
    WINDIVERT_ADDRESS addr;     // 包地址信息
    uint16_t local_port;        // 本地端口
    std::chrono::steady_clock::time_point timestamp;  // 时间戳
};

class PacketRedirector {
public:
    PacketRedirector();
    ~PacketRedirector();

    bool Start();
    void Stop();

private:
    void MainLoop();
    void SocketEventLoop();  // 新增：SOCKET事件处理循环
    void ProcessPacket(WINDIVERT_ADDRESS& addr, 
                      PWINDIVERT_IPHDR ip_header, 
                      PWINDIVERT_TCPHDR tcp_header);
    bool InitializeWinDivert();
    bool InitializeSocketDivert();  // 新增：初始化SOCKET层WinDivert
    void InitializeWinsock();

private:
    HANDLE m_handle;           // 网络层句柄
    HANDLE m_socketHandle;     // 新增：SOCKET层句柄
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    std::thread m_socketThread;  // 新增：SOCKET事件处理线程
    std::unique_ptr<ProxyServer> m_proxyServer;
}; 