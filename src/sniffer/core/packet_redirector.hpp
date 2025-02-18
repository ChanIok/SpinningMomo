#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include "windivert.h"
#include "sniffer/types/common_types.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

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

class PacketRedirector {
public:
    PacketRedirector();
    ~PacketRedirector();

    bool Start();
    void Stop();

private:
    void MainLoop();
    void ProcessPacket(WINDIVERT_ADDRESS& addr, 
                      PWINDIVERT_IPHDR ip_header, 
                      PWINDIVERT_TCPHDR tcp_header);
    bool InitializeWinDivert();
    void InitializeWinsock();

private:
    HANDLE m_handle;
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    std::unique_ptr<ProxyServer> m_proxyServer;
}; 