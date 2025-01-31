#pragma once

#include <httplib.h>
#include <memory>
#include <string>
#include <functional>
#include "thread_raii.hpp"

/**
 * @brief HTTP服务器类
 * 提供HTTP服务器功能，支持RESTful API
 */
class HttpServer {
public:
    /**
     * @brief 构造函数
     * @param host 服务器主机地址
     * @param port 服务器端口号
     */
    HttpServer(const std::string& host = "localhost", int port = 51206);
    ~HttpServer();

    /**
     * @brief 启动HTTP服务器
     * @return 启动是否成功
     */
    bool Start();

    /**
     * @brief 停止HTTP服务器
     */
    void Stop();

    /**
     * @brief 检查服务器是否正在运行
     * @return 服务器运行状态
     */
    bool IsRunning() const { return m_isRunning; }

private:
    /**
     * @brief 服务器线程主函数
     */
    void ServerThreadProc();
    
    /**
     * @brief 配置CORS（跨域资源共享）
     */
    void SetupCors();

    std::string m_host;              // 服务器主机地址
    int m_port;                      // 服务器端口号
    std::unique_ptr<httplib::Server> m_server;  // HTTP服务器实例
    std::atomic<bool> m_isRunning{false};       // 服务器运行状态
    ThreadRAII m_serverThread;       // 服务器工作线程
}; 
