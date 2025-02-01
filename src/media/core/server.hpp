#pragma once

#include <memory>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <uwebsockets/App.h>
#include "thread_raii.hpp"

/**
 * @brief 服务器类
 * 提供基于uWebSockets的HTTP服务器功能
 */
class Server {
public:
    /**
     * @brief 构造函数
     * @param host 服务器主机地址
     * @param port 服务器端口号
     */
    Server(const std::string& host = "localhost", int port = 51205);
    ~Server();

    /**
     * @brief 启动服务器
     * @return 启动是否成功
     */
    bool Start();

    /**
     * @brief 停止服务器
     */
    void Stop();

    /**
     * @brief 检查服务器是否正在运行
     * @return 服务器运行状态
     */
    bool IsRunning() const { return m_isRunning; }

    /**
     * @brief 获取App实例
     * @return App实例的引用
     */
    uWS::App& GetApp() { return *m_app; }

private:
    /**
     * @brief 服务器线程主函数
     */
    void ServerThreadProc();

    /**
     * @brief 配置CORS
     */
    void SetupCors();

    std::string m_host;                  // 服务器主机地址
    int m_port;                          // 服务器端口号
    std::unique_ptr<uWS::App> m_app;     // uWebSockets应用实例
    std::atomic<bool> m_isRunning{false};// 服务器运行状态
    us_listen_socket_t* m_listenSocket{nullptr}; // 监听socket
    ThreadRAII m_serverThread;           // 服务器工作线程，使用RAII管理
}; 