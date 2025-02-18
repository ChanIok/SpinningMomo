#pragma once
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "sniffer/types/common_types.hpp"

class SSLUtils {
public:
    // 从 TLS ClientHello 中提取 SNI
    static std::string extractSNI(const char* data, int length);
    
    // 创建自定义BIO方法
    static BIO_METHOD* createCustomBIOMethod();
    
    // 打印OpenSSL错误
    static void printOpenSSLError(const char* message);
    
    // 自定义接收函数
    static int CustomReceive(BIO* bio, char* buf, int sz);
    
    // 自定义发送函数
    static int CustomSend(BIO* bio, const char* buf, int sz);

private:
    static BIO_METHOD* s_customMethod;
}; 