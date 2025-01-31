#include "server.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "media/routes/route_manager.hpp"

using json = nlohmann::json;

HttpServer::HttpServer(const std::string& host, int port)
    : m_host(host)
    , m_port(port)
    , m_server(std::make_unique<httplib::Server>()) {
}

HttpServer::~HttpServer() {
    Stop();
}

bool HttpServer::Start() {
    if (m_isRunning) {
        spdlog::info("Server is already running");
        return true;
    }

    // 启动服务器线程
    try {
        m_serverThread = ThreadRAII([this]() { this->ServerThreadProc(); });
        spdlog::info("Server thread started successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start server thread: {}", e.what());
        return false;
    }
}

void HttpServer::Stop() {
    if (!m_isRunning) {
        return;
    }

    spdlog::info("Stopping HTTP server...");
    m_isRunning = false;

    // 停止服务器
    if (m_server) {
        m_server->stop();
    }
    
    spdlog::info("HTTP server stopped");
}

void HttpServer::ServerThreadProc() {
    // 配置服务器
    SetupCors();
    
    // 注册所有路由
    RouteManager::get_instance().register_routes(*m_server);
    
    // 启动服务器
    m_isRunning = true;
    spdlog::info("Starting HTTP server on {}:{}", m_host, m_port);
    
    if (!m_server->listen(m_host.c_str(), m_port)) {
        spdlog::error("Failed to start HTTP server on {}:{}", m_host, m_port);
        m_isRunning = false;
        return;
    }
    
    m_isRunning = false;
}

void HttpServer::SetupCors() {
    // 配置CORS中间件
    m_server->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        
        // 处理预检请求
        if (req.method == "OPTIONS") {
            res.status = 204;  // No Content
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    spdlog::info("CORS configured successfully");
} 