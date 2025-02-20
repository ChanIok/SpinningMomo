#include "packet_redirector.hpp"
#include "proxy_server.hpp"
#include "cert_manager.hpp"
#include "connection_manager.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <psapi.h>  // 用于获取进程信息
#include "media/utils/logger.hpp"  // 添加日志头文件
#include <TlHelp32.h>  // 添加 ToolHelp32 头文件
#include "process_manager.hpp"

PacketRedirector::PacketRedirector()
    : m_handle(INVALID_HANDLE_VALUE), m_socketHandle(INVALID_HANDLE_VALUE), m_running(false) {
    spdlog::info("PacketRedirector: Initialization started");
    
    // 1. 初始化 OpenSSL 环境
    OpenSSLEnvironment::getInstance();
    spdlog::debug("PacketRedirector: OpenSSL environment initialized");
    
    // 2. 初始化 Windows Socket
    InitializeWinsock();
    spdlog::debug("PacketRedirector: Winsock initialized");
    
    // 3. 初始化证书管理器
    if (!CertManager::getInstance().initialize()) {
        spdlog::error("PacketRedirector: Failed to initialize certificate manager");
        throw std::runtime_error("Failed to initialize certificates");
    }
    spdlog::debug("PacketRedirector: Certificate manager initialized");
    
    // 4. 等待证书安装完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 5. 创建代理服务器
    m_proxyServer = std::make_unique<ProxyServer>(ProxyConfig());
    spdlog::info("PacketRedirector: Initialization completed");
}

PacketRedirector::~PacketRedirector() {
    Stop();
}

bool PacketRedirector::Start() {
    if (m_running) {
        spdlog::warn("PacketRedirector: Already running");
        return true;
    }
    
    spdlog::info("PacketRedirector: Starting service");
    
    // 初始化连接管理器
    ConnectionManager::getInstance().start();
    spdlog::debug("PacketRedirector: Connection manager started");
    
    // 初始化WinDivert
    if (!InitializeWinDivert() || !InitializeSocketDivert()) {
        spdlog::error("PacketRedirector: WinDivert initialization failed");
        return false;
    }
    spdlog::debug("PacketRedirector: WinDivert initialized");

    // 启动代理服务器
    if (!m_proxyServer->Start()) {
        spdlog::error("PacketRedirector: Proxy server failed to start");
        return false;
    }
    spdlog::debug("PacketRedirector: Proxy server started");

    m_running = true;
    m_mainThread = std::thread(&PacketRedirector::MainLoop, this);
    m_socketThread = std::thread(&PacketRedirector::SocketEventLoop, this);
    spdlog::info("PacketRedirector: Service started successfully");
    return true;
}

void PacketRedirector::Stop() {
    if (!m_running) {
        return;
    }
    
    spdlog::info("PacketRedirector: Stopping service");
    m_running = false;
    
    // 关闭WinDivert句柄
    if (m_handle != INVALID_HANDLE_VALUE) {
        WinDivertClose(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
        spdlog::debug("PacketRedirector: Network layer WinDivert handle closed");
    }
    
    if (m_socketHandle != INVALID_HANDLE_VALUE) {
        WinDivertClose(m_socketHandle);
        m_socketHandle = INVALID_HANDLE_VALUE;
        spdlog::debug("PacketRedirector: Socket layer WinDivert handle closed");
    }
    
    // 等待线程结束
    if (m_mainThread.joinable()) {
        m_mainThread.join();
        spdlog::debug("PacketRedirector: Main thread stopped");
    }
    
    if (m_socketThread.joinable()) {
        m_socketThread.join();
        spdlog::debug("PacketRedirector: Socket event thread stopped");
    }

    // 停止连接管理器
    ConnectionManager::getInstance().stop();
    spdlog::debug("PacketRedirector: Connection manager stopped");
    
    // 停止代理服务器
    m_proxyServer->Stop();
    spdlog::debug("PacketRedirector: Proxy server stopped");
    
    spdlog::info("PacketRedirector: Service stopped completely");
}

bool PacketRedirector::InitializeWinDivert() {
    // 构建WinDivert过滤器字符串
    std::stringstream ss;
    ss << "ip and tcp and "  // 明确指定只捕获IPv4的TCP包
       << "(tcp.DstPort == " << TARGET_PORT 
       << " or tcp.DstPort == " << PROXY_PORT 
       << " or tcp.DstPort == " << ALT_PORT 
       << " or tcp.SrcPort == " << TARGET_PORT 
       << " or tcp.SrcPort == " << PROXY_PORT 
       << " or tcp.SrcPort == " << ALT_PORT << ")";
    
    std::string filter = ss.str();

    // 打开WinDivert句柄
    m_handle = WinDivertOpen(filter.c_str(), WINDIVERT_LAYER_NETWORK, 0, 0);
    if (m_handle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open WinDivert handle" << std::endl;
        return false;
    }

    return true;
}

bool PacketRedirector::InitializeSocketDivert() {
    // 构建SOCKET层过滤器字符串
    const char* socket_filter = 
        "tcp and "
        "(event == CONNECT or event == CLOSE) and "
        "localAddr != :: and remoteAddr != ::";
        
    m_socketHandle = WinDivertOpen(
        socket_filter,
        WINDIVERT_LAYER_SOCKET,
        1234,  // 优先级
        WINDIVERT_FLAG_SNIFF | WINDIVERT_FLAG_RECV_ONLY);
        
    if (m_socketHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open WinDivert socket handle" << std::endl;
        return false;
    }

    return true;
}

// 初始化Winsock
void PacketRedirector::InitializeWinsock() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

// 主循环
void PacketRedirector::MainLoop() {
    const UINT packetSize = 0xFFFF;
    std::vector<uint8_t> packet(packetSize);
    UINT readLen = 0;
    WINDIVERT_ADDRESS addr = {};

    while (m_running) {
        // 读取数据包
        if (!WinDivertRecv(m_handle, packet.data(), packetSize, &readLen, &addr)) {
            continue;
        }

        // 解析IP头和TCP头
        PWINDIVERT_IPHDR ip_header = nullptr;
        PWINDIVERT_IPV6HDR ipv6_header = nullptr;
        UINT8 protocol = 0;
        PWINDIVERT_ICMPHDR icmp_header = nullptr;
        PWINDIVERT_ICMPV6HDR icmpv6_header = nullptr;
        PWINDIVERT_TCPHDR tcp_header = nullptr;
        PWINDIVERT_UDPHDR udp_header = nullptr;
        PVOID payload = nullptr;
        UINT payload_len = 0;
        PVOID next_header = nullptr;
        UINT next_len = 0;

        WinDivertHelperParsePacket(packet.data(), readLen, 
                                  &ip_header, &ipv6_header, &protocol,
                                  &icmp_header, &icmpv6_header, &tcp_header, &udp_header,
                                  &payload, &payload_len, &next_header, &next_len);

        if (ip_header && tcp_header) {
            ProcessPacket(addr, ip_header, tcp_header);
        }

        // 重新注入数据包
        UINT sendLen = 0;
        WinDivertSend(m_handle, packet.data(), readLen, &sendLen, &addr);
    }
}

// 处理数据包
void PacketRedirector::ProcessPacket(WINDIVERT_ADDRESS& addr, 
                                   PWINDIVERT_IPHDR ip_header, 
                                   PWINDIVERT_TCPHDR tcp_header) 
{
    uint16_t local_port = addr.Outbound ? 
        ntohs(tcp_header->SrcPort) : ntohs(tcp_header->DstPort);
    uint16_t dest_port = addr.Outbound ? 
        ntohs(tcp_header->DstPort) : ntohs(tcp_header->SrcPort);
    
    spdlog::debug("PacketRedirector: Processing packet - Local Port: {}, Dest Port: {}, Flags: SYN={}, FIN={}, RST={}", 
                 local_port, dest_port, 
                 tcp_header->Syn ? 1 : 0,
                 tcp_header->Fin ? 1 : 0,
                 tcp_header->Rst ? 1 : 0);

    auto& conn_mgr = ConnectionManager::getInstance();
    auto* conn = conn_mgr.getConnection(local_port);

    // 判断是否需要处理这个包
    bool is_target = conn_mgr.hasConnection(local_port);
    bool is_proxy_related = (local_port == PROXY_PORT || local_port == ALT_PORT || 
                           dest_port == PROXY_PORT || dest_port == ALT_PORT);
    
    // 只处理目标进程的包或代理相关的包
    if (!is_target && !is_proxy_related) {
        return;
    }

    // 如果是目标进程的包，更新连接活动时间
    if (is_target) {
        conn_mgr.updateConnection(local_port);
    }
    
    // 处理数据包重定向逻辑
    if (addr.Outbound) {
        if (ntohs(tcp_header->DstPort) == TARGET_PORT) {
            spdlog::debug("PacketRedirector: Redirecting to proxy port: {} -> {}", 
                         ntohs(tcp_header->DstPort), PROXY_PORT);
            UINT32 dst_addr = ip_header->DstAddr;
            tcp_header->DstPort = htons(PROXY_PORT);
            ip_header->DstAddr = ip_header->SrcAddr;
            ip_header->SrcAddr = dst_addr;
            addr.Outbound = FALSE;
        }
        else if (ntohs(tcp_header->SrcPort) == PROXY_PORT) {
            spdlog::debug("PacketRedirector: Reflecting: proxy port -> target port: {} -> {}", 
                         PROXY_PORT, TARGET_PORT);
            UINT32 dst_addr = ip_header->DstAddr;
            tcp_header->SrcPort = htons(TARGET_PORT);
            ip_header->DstAddr = ip_header->SrcAddr;
            ip_header->SrcAddr = dst_addr;
            addr.Outbound = FALSE;
        }
        else if (ntohs(tcp_header->DstPort) == ALT_PORT) {
            spdlog::debug("PacketRedirector: Redirecting: alt port -> target port: {} -> {}", 
                         ALT_PORT, TARGET_PORT);
            tcp_header->DstPort = htons(TARGET_PORT);
        }
    }
    else {
        if (ntohs(tcp_header->SrcPort) == TARGET_PORT) {
            spdlog::debug("PacketRedirector: Redirecting: target port -> alt port: {} -> {}", 
                         TARGET_PORT, ALT_PORT);
            tcp_header->SrcPort = htons(ALT_PORT);
        }
    }

    // 重新计算校验和
    WinDivertHelperCalcChecksums(
        reinterpret_cast<PVOID>(ip_header),
        sizeof(WINDIVERT_IPHDR) + sizeof(WINDIVERT_TCPHDR),
        &addr,
        0
    );
}

// 处理SOCKET层事件
void PacketRedirector::SocketEventLoop() {
    auto& process_mgr = ProcessManager::getInstance();
    auto& conn_mgr = ConnectionManager::getInstance();
    
    spdlog::info("PacketRedirector: Socket event loop started");
    
    while (m_running) {
        WINDIVERT_ADDRESS addr;
        if (!WinDivertRecv(m_socketHandle, NULL, 0, NULL, &addr)) {
            continue;
        }
        
        if (addr.Event == WINDIVERT_EVENT_SOCKET_CONNECT) {
            DWORD process_id = addr.Socket.ProcessId;
            
            // 快速判断是否为目标进程
            if (process_mgr.isTargetProcess(process_id)) {
                conn_mgr.addConnection(
                    addr.Socket.LocalPort,
                    addr.Socket.RemoteAddr[0],
                    addr.Socket.RemotePort
                );
                
                spdlog::info("PacketRedirector: Target process connection added - Port: {}", 
                            addr.Socket.LocalPort);
            }
        }
        else if (addr.Event == WINDIVERT_EVENT_SOCKET_CLOSE) {
            if (conn_mgr.hasConnection(addr.Socket.LocalPort)) {
                conn_mgr.removeConnection(addr.Socket.LocalPort);
                spdlog::debug("PacketRedirector: Connection removed - Port: {}", 
                             addr.Socket.LocalPort);
            }
        }
    }
    
    spdlog::info("PacketRedirector: Socket event loop ended");
}