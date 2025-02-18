#include "ssl_utils.hpp"
#include <iostream>

BIO_METHOD* SSLUtils::s_customMethod = nullptr;

std::string SSLUtils::extractSNI(const char* data, int length) {
    if (length < 43) return "";  // TLS记录头(5) + Handshake头(4) + ClientHello固定部分(34)
    
    // 检查是否是 TLS ClientHello
    if ((unsigned char)data[0] != 0x16 ||  // Handshake
        (unsigned char)data[5] != 0x01) {  // ClientHello
        return "";
    }

    // 跳过固定部分
    const unsigned char* p = (const unsigned char*)data + 43;
    const unsigned char* end = (const unsigned char*)data + length;
    
    // 跳过 Session ID
    if (p >= end) return "";
    int sessionIDLength = *p;
    p += 1 + sessionIDLength;
    
    // 跳过 Cipher Suites
    if (p + 2 > end) return "";
    int cipherSuitesLength = (p[0] << 8) | p[1];
    p += 2 + cipherSuitesLength;
    
    // 跳过 Compression Methods
    if (p + 1 > end) return "";
    int compressionMethodsLength = *p;
    p += 1 + compressionMethodsLength;
    
    // 检查是否有扩展部分
    if (p + 2 > end) return "";
    int extensionsLength = (p[0] << 8) | p[1];
    p += 2;
    
    // 遍历所有扩展
    while (p + 4 <= end) {
        int extensionType = (p[0] << 8) | p[1];
        int extensionLength = (p[2] << 8) | p[3];
        p += 4;
        
        // SNI 扩展类型是 0
        if (extensionType == 0 && p + extensionLength <= end) {
            p += 2;  // 跳过 SNI list length
            if (*p++ != 0) continue;  // name_type 必须是 0 (hostname)
            int hostnameLength = (p[0] << 8) | p[1];
            p += 2;
            if (p + hostnameLength <= end) {
                return std::string((const char*)p, hostnameLength);
            }
        }
        p += extensionLength;
    }
    
    return "";
}

void SSLUtils::printOpenSSLError(const char* message) {
    std::cerr << message << ": ";
    ERR_print_errors_fp(stderr);
    std::cerr << std::endl;
}

BIO_METHOD* SSLUtils::createCustomBIOMethod() {
    if (s_customMethod) return s_customMethod;

    s_customMethod = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, 
                                 "Custom Socket BIO");
    if (!s_customMethod) return nullptr;

    BIO_meth_set_write(s_customMethod, CustomSend);
    BIO_meth_set_read(s_customMethod, CustomReceive);
    BIO_meth_set_ctrl(s_customMethod, [](BIO* bio, int cmd, long num, void* ptr) -> long {
        if (cmd == BIO_CTRL_FLUSH) return 1;
        return 0;
    });
    BIO_meth_set_create(s_customMethod, [](BIO* bio) -> int {
        BIO_set_init(bio, 1);
        return 1;
    });
    BIO_meth_set_destroy(s_customMethod, [](BIO* bio) -> int {
        return 1;
    });

    return s_customMethod;
}

int SSLUtils::CustomReceive(BIO* bio, char* buf, int sz) {
    CustomIOCtx* io_ctx = (CustomIOCtx*)BIO_get_data(bio);
    
    // 如果还有初始数据未处理完
    if (!io_ctx->used_initial && io_ctx->initial_data) {
        int remaining = io_ctx->initial_len - io_ctx->initial_offset;
        if (remaining <= 0) {
            io_ctx->used_initial = true;
            return recv(io_ctx->socket, buf, sz, 0);
        }

        int read_size = (remaining < sz) ? remaining : sz;
        memcpy(buf, io_ctx->initial_data + io_ctx->initial_offset, read_size);
        io_ctx->initial_offset += read_size;
        
        if (io_ctx->initial_offset >= io_ctx->initial_len) {
            io_ctx->used_initial = true;
        }

        return read_size;
    }
    
    return recv(io_ctx->socket, buf, sz, 0);
}

int SSLUtils::CustomSend(BIO* bio, const char* buf, int sz) {
    CustomIOCtx* io_ctx = (CustomIOCtx*)BIO_get_data(bio);
    return send(io_ctx->socket, buf, sz, 0);
} 