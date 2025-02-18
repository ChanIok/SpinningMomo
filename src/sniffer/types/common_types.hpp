#pragma once
#include <string>
#include <vector>
#include <winsock2.h>
#include <openssl/ssl.h>
#include "core/win_config.hpp"

// 常量定义
constexpr uint16_t TARGET_PORT = 12101;
constexpr uint16_t PROXY_PORT = 51207;
constexpr uint16_t ALT_PORT = 51208;
constexpr size_t MAXBUF = 0xFFFF;

// 证书相关常量
const char* const CERT_DIR = "certs";
const char* const CA_CERT = "certs/ca-cert.pem";
const char* const CA_KEY = "certs/ca-key.pem";

// 支持的域名列表
const std::vector<std::string> SUPPORTED_DOMAINS = {
    "apm.papegames.com",
    "x6cn-clickhouse.nuanpaper.com"
};

// 自定义IO上下文
struct CustomIOCtx {
    const char* initial_data;    // 初始数据（ClientHello）
    int initial_len;            // 初始数据长度
    int initial_offset;         // 当前读取位置
    SOCKET socket;              // socket句柄
    bool used_initial;          // 是否已使用完初始数据
};

// 代理配置
struct ProxyConfig {
    ProxyConfig(uint16_t proxy_port = PROXY_PORT, uint16_t alt_port = ALT_PORT)
        : proxy_port(proxy_port), alt_port(alt_port) {}
    uint16_t proxy_port;
    uint16_t alt_port;
};

// 代理连接配置
struct ProxyConnectionConfig {
    SOCKET s;             // 客户端套接字
    uint16_t alt_port;    // 备用端口
    in_addr dest;         // 目标地址
};

// 代理传输配置
struct ProxyTransferConfig {
    ProxyTransferConfig(bool inbound, SOCKET s, SOCKET t, SSL* ssl_s = nullptr, SSL* ssl_t = nullptr)
        : inbound(inbound), s(s), t(t), ssl_s(ssl_s), ssl_t(ssl_t) {}
    
    bool inbound;      // 是否为入站流量
    SOCKET s;          // 源套接字
    SOCKET t;          // 目标套接字
    SSL* ssl_s;        // 源SSL连接
    SSL* ssl_t;        // 目标SSL连接
}; 