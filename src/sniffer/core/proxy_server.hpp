#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include <fstream>
#include "sniffer/types/common_types.hpp"

// HTTP消息结构体
struct HttpMessage {
    bool header_parsed = false;
    size_t content_length = 0;
    size_t header_end = 0;
    std::string data;

    bool isComplete() const {
        return header_parsed && data.length() >= (header_end + 4 + content_length);
    }
    
    void reset() {
        header_parsed = false;
        content_length = 0;
        header_end = 0;
        data.clear();
    }
};

class ProxyServer {
public:
    ProxyServer(const ProxyConfig& config);
    ~ProxyServer();

    bool Start();
    void Stop();

private:
    void MainLoop();
    void HandleConnection(std::shared_ptr<ProxyConnectionConfig> config);
    void HandleProxyTransfer(std::shared_ptr<ProxyTransferConfig> config);
    bool InitializeSocket();
    void LogDecryptedTraffic(const std::string& data, bool isRequest, const char* hostname);
    size_t ParseContentLength(const std::string& headers);

private:
    ProxyConfig m_config;
    SOCKET m_serverSocket;
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    std::ofstream m_logFile;
}; 