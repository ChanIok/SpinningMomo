#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <uwebsockets/App.h>
#include "thread_raii.hpp"

/**
 * @brief WebUI服务器类
 */
class Server {
public:
    /**
     * @brief 构造函数
     * @param host 服务器主机地址
     * @param port 服务器端口号
     */
    Server(const std::string& host = "0.0.0.0", int port = 51205);
    ~Server();

    /**
     * @brief 初始化服务器
     * @return 初始化是否成功
     */
    bool Initialize();

    /**
     * @brief 停止服务器
     */
    void Stop();

    /**
     * @brief 检查服务器是否正在运行
     * @return 服务器运行状态
     */
    bool IsRunning() const { return m_isRunning; }

private:
    // 基本配置
    std::string m_host;
    int m_port;
    std::atomic<bool> m_isRunning{false};
    us_listen_socket_t* m_listenSocket{nullptr};

    // 服务器线程
    ThreadRAII m_serverThread;
    
    // CORS 配置
    static void SetupCors(uWS::App& app);
}; 