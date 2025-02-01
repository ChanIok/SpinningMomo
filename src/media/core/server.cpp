#include "server.hpp"
#include "media/routes/route_manager.hpp"
#include "media/services/initialization_service.hpp"
#include <spdlog/spdlog.h>

Server::Server(const std::string& host, int port)
    : m_host(host)
    , m_port(port) {
}

Server::~Server() {
    Stop();
}

bool Server::Initialize() {
    if (m_isRunning) {
        OutputDebugStringA("Server already running\n");
        return true;
    }

    try {
        OutputDebugStringA("Initializing web server...\n");
        m_isRunning = true;
        
        // 在新线程中创建和运行服务器
        m_serverThread = ThreadRAII([this]() {
            OutputDebugStringA("Server thread starting...\n");
            
            // 在线程内创建App实例
            auto app = std::make_unique<uWS::App>();
            
            // 初始化服务
            try {
                InitializationService::get_instance().initialize();
            } catch (const std::exception& e) {
                spdlog::error("Failed to initialize services: {}", e.what());
                m_isRunning = false;
                return;
            }
            
            // 注册路由
            RouteManager::get_instance().register_routes(*app);
            
            // 配置CORS
            SetupCors(*app);
            
            // 启动监听
            app->listen(m_host, m_port, [this](auto* socket) {
                if (socket) {
                    m_listenSocket = socket;
                    spdlog::info("Server is listening on {}:{}", 
                        m_host, m_port);
                } else {
                    spdlog::error("Failed to listen on {}:{}", 
                        m_host, m_port);
                    m_isRunning = false;
                }
            });

            // 如果监听成功，运行事件循环
            if (m_isRunning) {
                app->run();
            }
            
            m_isRunning = false;
        });
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize server: {}", e.what());
        return false;
    }
}

void Server::Stop() {
    if (!m_isRunning) {
        return;
    }

    // 关闭监听socket会导致事件循环退出
    if (m_listenSocket) {
        us_listen_socket_close(0, m_listenSocket);
        m_listenSocket = nullptr;
    }
    
    // ThreadRAII的析构函数会自动等待线程结束
    m_isRunning = false;
}

void Server::SetupCors(uWS::App& app) {
    // 配置全局CORS中间件
    app.options("/*", [](auto* res, auto* req) {
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
        res->writeStatus("204 No Content");
        res->end();
    });
} 