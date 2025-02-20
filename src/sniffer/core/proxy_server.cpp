#include "proxy_server.hpp"
#include "cert_manager.hpp"
#include "sniffer/utils/ssl_utils.hpp"
#include "sniffer/utils/http_parser.hpp"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>

ProxyServer::ProxyServer(const ProxyConfig& config)
    : m_config(config), m_serverSocket(INVALID_SOCKET), m_running(false) {
    // 创建日志目录
    std::filesystem::create_directories("logs");
    
    // 创建日志文件
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "logs/proxy_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".txt";
    m_logFile.open(ss.str(), std::ios::out | std::ios::app);
}

ProxyServer::~ProxyServer() {
    Stop();
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

bool ProxyServer::Start() {
    if (m_running) return true;
    
    // 确保证书管理器已初始化
    if (!CertManager::getInstance().initialize()) {
        if (m_logFile.is_open()) {
            m_logFile << "Failed to initialize certificate manager" << std::endl;
            m_logFile.flush();
        }
        return false;
    }

    if (!InitializeSocket()) {
        return false;
    }

    // 添加启动日志
    if (m_logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        m_logFile << "\n=== Proxy Server Started at " 
                 << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") 
                 << " ===" << std::endl;
        m_logFile << "Listening on port: " << m_config.proxy_port << std::endl;
        m_logFile << "Alternative port: " << m_config.alt_port << std::endl;
        m_logFile.flush();
    }

    m_running = true;
    m_mainThread = std::thread(&ProxyServer::MainLoop, this);
    return true;
}

void ProxyServer::Stop() {
    if (!m_running) return;
    
    m_running = false;
    if (m_serverSocket != INVALID_SOCKET) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }
    
    if (m_mainThread.joinable()) {
        m_mainThread.join();
    }
}

bool ProxyServer::InitializeSocket() {
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket == INVALID_SOCKET) {
        return false;
    }

    // 设置套接字选项（允许地址重用）
    int reuse = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR) {
        closesocket(m_serverSocket);
        return false;
    }

    // 绑定服务器地址
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_config.proxy_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(m_serverSocket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(m_serverSocket);
        return false;
    }

    // 开始监听连接
    if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(m_serverSocket);
        return false;
    }

    return true;
}

void ProxyServer::MainLoop() {
    while (m_running) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        
        // 接受客户端连接
        SOCKET client_socket = accept(m_serverSocket, 
            reinterpret_cast<SOCKADDR*>(&client_addr), &addr_len);

        if (client_socket == INVALID_SOCKET) {
            if (m_running) {
                // 只有在服务器正常运行时才记录错误
                if (m_logFile.is_open()) {
                    m_logFile << "Failed to accept connection: " << WSAGetLastError() << std::endl;
                    m_logFile.flush();
                }
            }
            continue;
        }

        // 为新连接创建配置
        auto conn_config = std::make_shared<ProxyConnectionConfig>();
        conn_config->s = client_socket;
        conn_config->alt_port = m_config.alt_port;
        conn_config->dest = client_addr.sin_addr;

        // 启动新的连接处理线程
        std::thread(&ProxyServer::HandleConnection, this, conn_config).detach();
    }
}

void ProxyServer::HandleConnection(std::shared_ptr<ProxyConnectionConfig> config) {
    try {
        // 读取 ClientHello
        char buffer[MAXBUF];
        int bytesRead = recv(config->s, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            if (m_logFile.is_open()) {
                m_logFile << "Failed to read ClientHello, error: " << WSAGetLastError() << std::endl;
                m_logFile.flush();
            }
            throw std::runtime_error("Failed to read ClientHello");
        }

        // 提取域名
        std::string hostname = SSLUtils::extractSNI(buffer, bytesRead);
        
        // 创建目标服务器套接字
        SOCKET target_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (target_socket == INVALID_SOCKET) {
            if (m_logFile.is_open()) {
                m_logFile << "Failed to create target socket: " << WSAGetLastError() << std::endl;
                m_logFile.flush();
            }
            throw std::runtime_error("Failed to create target socket");
        }

        // 设置目标服务器地址
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config->alt_port);
        addr.sin_addr = config->dest;
        
        // 连接到目标服务器
        if (connect(target_socket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            if (m_logFile.is_open()) {
                m_logFile << "Failed to connect to target server: " << WSAGetLastError() << std::endl;
                m_logFile.flush();
            }
            closesocket(target_socket);
            throw std::runtime_error("Failed to connect to target server");
        }

        if (m_logFile.is_open()) {
            m_logFile << "Connected to target server: " 
                     << inet_ntoa(addr.sin_addr) << ":" << config->alt_port << std::endl;
            m_logFile.flush();
        }
        
        // 检查是否是支持的域名
        bool supported = false;
        for (const auto& domain : SUPPORTED_DOMAINS) {
            if (hostname == domain) {
                supported = true;
                break;
            }
        }
        
        if (!supported) {
            if (m_logFile.is_open()) {
                m_logFile << "Bypassing non-supported domain: " << hostname << std::endl;
                m_logFile.flush();
            }

            // 转发 ClientHello
            send(target_socket, buffer, bytesRead, 0);

            // 创建双向转发通道（不使用SSL）
            auto config1 = std::make_shared<ProxyTransferConfig>(false, config->s, target_socket);
            auto config2 = std::make_shared<ProxyTransferConfig>(true, target_socket, config->s);

            // 启动数据转发
            std::thread t1(&ProxyServer::HandleProxyTransfer, this, config1);
            HandleProxyTransfer(config2);
            t1.join();

            // 清理资源
            closesocket(config->s);
            closesocket(target_socket);
            return;
        }

        if (m_logFile.is_open()) {
            m_logFile << "\nIntercepting connection to: " << hostname << std::endl;
            m_logFile.flush();
        }

        // 获取SSL上下文
        SSL_CTX* serverCtx = CertManager::getInstance().createServerContext();
        SSL_CTX* clientCtx = CertManager::getInstance().createClientContext();

        if (!serverCtx || !clientCtx) {
            if (m_logFile.is_open()) {
                m_logFile << "Failed to create SSL context" << std::endl;
                m_logFile.flush();
            }
            closesocket(target_socket);
            throw std::runtime_error("Failed to create SSL context");
        }

        // 创建SSL连接
        SSL* browser_ssl = SSL_new(serverCtx);
        SSL* server_ssl = SSL_new(clientCtx);

        if (!browser_ssl || !server_ssl) {
            if (m_logFile.is_open()) {
                m_logFile << "Failed to create SSL connection" << std::endl;
                unsigned long err;
                while ((err = ERR_get_error()) != 0) {
                    m_logFile << "OpenSSL error: " << ERR_error_string(err, nullptr) << std::endl;
                }
                m_logFile.flush();
            }
            SSL_CTX_free(serverCtx);
            SSL_CTX_free(clientCtx);
            closesocket(target_socket);
            throw std::runtime_error("Failed to create SSL connection");
        }

        // 设置客户端SSL的自定义IO
        CustomIOCtx client_io_ctx = {
            buffer,           // 初始数据（ClientHello）
            bytesRead,        // 初始数据长度
            0,               // 初始偏移量
            config->s,        // 客户端socket
            false            // 未使用完初始数据
        };
        
        // 设置SSL实例的IO函数和上下文
        BIO* bio = BIO_new(SSLUtils::createCustomBIOMethod());
        if (!bio) {
            if (m_logFile.is_open()) {
                m_logFile << "Failed to create BIO" << std::endl;
                m_logFile.flush();
            }
            throw std::runtime_error("Failed to create BIO");
        }

        BIO_set_data(bio, &client_io_ctx);
        SSL_set_bio(browser_ssl, bio, bio);
        SSL_set_fd(server_ssl, target_socket);
        
        // 先与客户端建立 SSL 连接
        int accept_result = SSL_accept(browser_ssl);
        if (accept_result <= 0) {
            int ssl_error = SSL_get_error(browser_ssl, accept_result);
            if (m_logFile.is_open()) {
                m_logFile << "SSL accept failed with error: " << ssl_error << std::endl;
                m_logFile << "OpenSSL error details:" << std::endl;
                unsigned long err;
                while ((err = ERR_get_error()) != 0) {
                    m_logFile << "OpenSSL error: " << ERR_error_string(err, nullptr) << std::endl;
                }
                m_logFile.flush();
            }
            throw std::runtime_error("Failed SSL handshake with client");
        }

        // 然后与服务器建立 SSL 连接
        if (SSL_connect(server_ssl) <= 0) {
            if (m_logFile.is_open()) {
                m_logFile << "Failed SSL handshake with target server" << std::endl;
                unsigned long err;
                while ((err = ERR_get_error()) != 0) {
                    m_logFile << "OpenSSL error: " << ERR_error_string(err, nullptr) << std::endl;
                }
                m_logFile.flush();
            }
            throw std::runtime_error("Failed SSL handshake with target server");
        }

        // 创建双向数据传输配置
        auto config1 = std::make_shared<ProxyTransferConfig>(false, config->s, target_socket, browser_ssl, server_ssl);
        auto config2 = std::make_shared<ProxyTransferConfig>(true, target_socket, config->s, server_ssl, browser_ssl);

        // 启动数据传输线程
        std::thread t1(&ProxyServer::HandleProxyTransfer, this, config1);
        HandleProxyTransfer(config2);
        t1.join();

        // 清理资源
        SSL_free(browser_ssl);
        SSL_free(server_ssl);
        SSL_CTX_free(serverCtx);
        SSL_CTX_free(clientCtx);
        closesocket(config->s);
        closesocket(target_socket);
    }
    catch (const std::exception& e) {
        if (m_logFile.is_open()) {
            m_logFile << "Proxy connection error: " << e.what() << std::endl;
            m_logFile.flush();
        }
    }
}

size_t ProxyServer::ParseContentLength(const std::string& headers) {
    const std::string content_length_header = "Content-Length: ";
    size_t pos = headers.find(content_length_header);
    if (pos == std::string::npos) {
        return 0;
    }
    
    pos += content_length_header.length();
    size_t end_pos = headers.find("\r\n", pos);
    if (end_pos == std::string::npos) {
        return 0;
    }
    
    try {
        return std::stoull(headers.substr(pos, end_pos - pos));
    } catch (...) {
        return 0;
    }
}

void ProxyServer::HandleProxyTransfer(std::shared_ptr<ProxyTransferConfig> config) {
    try {
        std::vector<char> buffer(MAXBUF);
        HttpMessage message;

        while (true) {
            // 接收数据
            int recv_len;
            if (config->ssl_s) {
                recv_len = SSL_read(config->ssl_s, buffer.data(), buffer.size());
            } else {
                recv_len = recv(config->s, buffer.data(), buffer.size(), 0);
            }

            if (recv_len <= 0) {
                if (config->ssl_s) {
                    SSL_shutdown(config->ssl_s);
                }
                shutdown(config->s, SD_RECEIVE);
                shutdown(config->t, SD_SEND);
                break;
            }

            // 累积数据
            message.data.append(buffer.data(), recv_len);

            // 如果还没解析头部，尝试解析
            if (!message.header_parsed) {
                size_t header_end = message.data.find("\r\n\r\n");
                if (header_end != std::string::npos) {
                    message.header_end = header_end;
                    // 解析 Content-Length
                    const std::string content_length_header = "Content-Length: ";
                    size_t pos = message.data.find(content_length_header);
                    
                    if (pos != std::string::npos && pos < header_end) {
                        pos += content_length_header.length();
                        size_t end_pos = message.data.find("\r\n", pos);
                        
                        if (end_pos != std::string::npos && end_pos <= header_end) {
                            try {
                                message.content_length = std::stoull(message.data.substr(pos, end_pos - pos));
                                message.header_parsed = true;
                            } catch (const std::exception& e) {
                                if (m_logFile.is_open()) {
                                    m_logFile << "\n[ERROR] Failed to parse Content-Length value: " << e.what() << "\n";
                                    m_logFile.flush();
                                }
                            } catch (...) {
                                if (m_logFile.is_open()) {
                                    m_logFile << "\n[ERROR] Failed to parse Content-Length value with unknown error\n";
                                    m_logFile.flush();
                                }
                            }
                        }
                    }
                }
            }

            // 检查是否收到完整消息
            if (message.isComplete()) {
                // 记录完整的HTTP消息
                const char* hostname = config->ssl_s ? 
                    SSL_get_servername(config->ssl_s, TLSEXT_NAMETYPE_host_name) : "unknown";
                
                if (m_logFile.is_open()) {
                    auto now = std::chrono::system_clock::now();
                    auto time = std::chrono::system_clock::to_time_t(now);
                    m_logFile << "\n[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") 
                            << "] " << (!config->inbound ? "REQUEST" : "RESPONSE") 
                            << " " << (hostname ? hostname : "unknown") << "\n";
                    
                    // 写入完整消息
                    m_logFile.write(message.data.data(), message.data.length());
                    m_logFile << "\n----------------------------------------\n";
                    m_logFile.flush();
                }
                
                message.reset();
            }

            // 发送数据到目标
            int sent_total = 0;
            while (sent_total < recv_len) {
                int sent;
                if (config->ssl_t) {
                    sent = SSL_write(config->ssl_t, buffer.data() + sent_total, 
                                   recv_len - sent_total);
                } else {
                    sent = send(config->t, buffer.data() + sent_total, 
                              recv_len - sent_total, 0);
                }
                if (sent <= 0) {
                    throw std::runtime_error("Failed to send data");
                }
                sent_total += sent;
            }
        }
    }
    catch (const std::exception& e) {
        if (m_logFile.is_open()) {
            m_logFile << "Transfer error: " << e.what() << std::endl;
            m_logFile.flush();
        }
        
        if (config->ssl_s) SSL_shutdown(config->ssl_s);
        if (config->ssl_t) SSL_shutdown(config->ssl_t);
        shutdown(config->s, SD_BOTH);
        shutdown(config->t, SD_BOTH);
    }
}