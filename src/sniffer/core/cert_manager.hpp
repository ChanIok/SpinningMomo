#pragma once
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include "sniffer/types/common_types.hpp"

class CertManager {
public:
    static CertManager& getInstance() {
        static CertManager instance;
        return instance;
    }

    bool initialize();
    SSL_CTX* createServerContext();
    SSL_CTX* createClientContext();
    
    // SNI 回调函数
    static int sni_callback(SSL *ssl, int *al, void *arg);
    
    // 证书选择函数
    int select_certificate(SSL *ssl, const char *hostname);

private:
    CertManager();
    ~CertManager();
    
    // 禁用拷贝构造和赋值
    CertManager(const CertManager&) = delete;
    CertManager& operator=(const CertManager&) = delete;

    bool loadOrCreateCA();
    bool loadOrCreateSiteCerts();
    bool loadOrCreateSiteCert(const char* domain, X509*& cert, EVP_PKEY*& key);
    bool installRootCertificate();
    
    // 证书生成相关
    EVP_PKEY* generateRSAKey(int bits = 2048);
    X509* generateRootCertificate();
    X509* generateSiteCertificate(const char* domain, EVP_PKEY* key);
    bool addX509Extension(X509* cert, X509V3_CTX* ctx, int nid, const char* value);

private:
    X509* rootCA;        // 根证书
    EVP_PKEY* rootKey;   // 根证书私钥
    X509* siteCert;      // apm.papegames.com 的证书
    EVP_PKEY* siteKey;   // 站点证书1私钥
    X509* siteCert2;     // x6cn-clickhouse.nuanpaper.com 的证书
    EVP_PKEY* siteKey2;  // 站点证书2私钥
}; 