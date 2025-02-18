#include "packet_redirector.hpp"
#include "proxy_server.hpp"
#include "cert_manager.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

PacketRedirector::PacketRedirector()
    : m_handle(INVALID_HANDLE_VALUE), m_running(false) {
    // 1. 初始化 OpenSSL 环境
    OpenSSLEnvironment::getInstance();
    
    // 2. 初始化 Windows Socket
    InitializeWinsock();
    
    // 3. 初始化证书管理器
    if (!CertManager::getInstance().initialize()) {
        throw std::runtime_error("Failed to initialize certificates");
    }
    
    // 4. 等待证书安装完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 5. 创建代理服务器
    m_proxyServer = std::make_unique<ProxyServer>(ProxyConfig());
}

PacketRedirector::~PacketRedirector() {
    Stop();
}

bool PacketRedirector::Start() {
    if (m_running) return true;
    
    if (!InitializeWinDivert()) {
        return false;
    }

    // 启动代理服务器
    if (!m_proxyServer->Start()) {
        return false;
    }

    m_running = true;
    m_mainThread = std::thread(&PacketRedirector::MainLoop, this);
    return true;
}

void PacketRedirector::Stop() {
    if (!m_running) return;
    
    m_running = false;
    if (m_handle != INVALID_HANDLE_VALUE) {
        WinDivertClose(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
    
    if (m_mainThread.joinable()) {
        m_mainThread.join();
    }

    m_proxyServer->Stop();
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

void PacketRedirector::InitializeWinsock() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

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

void PacketRedirector::ProcessPacket(WINDIVERT_ADDRESS& addr, 
                                   PWINDIVERT_IPHDR ip_header, 
                                   PWINDIVERT_TCPHDR tcp_header) {
    if (addr.Outbound) {
        if (ntohs(tcp_header->DstPort) == TARGET_PORT) {  // 12101端口
            // 重定向到代理端口
            UINT32 dst_addr = ip_header->DstAddr;
            tcp_header->DstPort = htons(PROXY_PORT);
            ip_header->DstAddr = ip_header->SrcAddr;
            ip_header->SrcAddr = dst_addr;
            addr.Outbound = FALSE;
        }
        else if (ntohs(tcp_header->SrcPort) == PROXY_PORT) {
            // 反射：代理端口 -> 目标端口
            UINT32 dst_addr = ip_header->DstAddr;
            tcp_header->SrcPort = htons(TARGET_PORT);
            ip_header->DstAddr = ip_header->SrcAddr;
            ip_header->SrcAddr = dst_addr;
            addr.Outbound = FALSE;
        }
        else if (ntohs(tcp_header->DstPort) == ALT_PORT) {
            // 重定向：备用端口 -> 目标端口
            tcp_header->DstPort = htons(TARGET_PORT);
        }
    }
    else {
        if (ntohs(tcp_header->SrcPort) == TARGET_PORT) {
            // 重定向：目标端口 -> 备用端口
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