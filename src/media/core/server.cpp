#include "server.hpp"
#include "media/routes/route_manager.hpp"
#include <spdlog/spdlog.h>

Server::Server(const std::string& host, int port)
    : m_host(host)
    , m_port(port)
    , m_app(std::make_unique<uWS::App>()) {
}

Server::~Server() {
    Stop();
}

bool Server::Start() {
    if (m_isRunning) {
        return true;
    }

    try {
        // 注册所有路由
        RouteManager::get_instance().register_routes(*m_app);
        
        // 配置CORS
        SetupCors();
        
        m_isRunning = true;
        
        // 直接在当前线程启动服务器
        m_app->listen(m_host, m_port, [this](auto* socket) {
            if (socket) {
                m_listenSocket = socket;
                spdlog::info("Server is listening on {}:{}", m_host, m_port);
            } else {
                spdlog::error("Failed to listen on {}:{}", m_host, m_port);
                m_isRunning = false;
            }
        }).run();  // 直接运行事件循环

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start server: {}", e.what());
        return false;
    }
}

void Server::Stop() {
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;

    // 关闭监听socket
    if (m_listenSocket) {
        us_listen_socket_close(0, m_listenSocket);
        m_listenSocket = nullptr;
    }
}

void Server::SetupCors() {
    // 配置全局CORS中间件
    m_app->options("/*", [](auto* res, auto* req) {
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
        res->writeStatus("204 No Content");
        res->end();
    });
} 