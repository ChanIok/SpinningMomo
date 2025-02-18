#include "cert_manager.hpp"
#include "sniffer/utils/ssl_utils.hpp"
#include <filesystem>
#include <iostream>

CertManager::CertManager() : rootCA(nullptr), rootKey(nullptr), 
    siteCert(nullptr), siteKey(nullptr), siteCert2(nullptr), siteKey2(nullptr) {}

CertManager::~CertManager() {
    if (rootCA) X509_free(rootCA);
    if (rootKey) EVP_PKEY_free(rootKey);
    if (siteCert) X509_free(siteCert);
    if (siteKey) EVP_PKEY_free(siteKey);
    if (siteCert2) X509_free(siteCert2);
    if (siteKey2) EVP_PKEY_free(siteKey2);
}

bool CertManager::initialize() {
    // 创建证书目录
    std::filesystem::create_directories(CERT_DIR);
    
    // 生成或加载CA证书
    if (!loadOrCreateCA()) return false;
    // 生成或加载域名证书
    if (!loadOrCreateSiteCerts()) return false;
    // 安装CA证书到系统
    return installRootCertificate();
}

SSL_CTX* CertManager::createServerContext() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) return nullptr;

    // 设置 SNI 回调
    SSL_CTX_set_tlsext_servername_callback(ctx, sni_callback);
    SSL_CTX_set_tlsext_servername_arg(ctx, this);

    // 设置根证书链
    if (!SSL_CTX_add_extra_chain_cert(ctx, X509_dup(rootCA))) {
        SSL_CTX_free(ctx);
        return nullptr;
    }

    return ctx;
}

SSL_CTX* CertManager::createClientContext() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return nullptr;
    
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    return ctx;
}

int CertManager::sni_callback(SSL *ssl, int *al, void *arg) {
    const char *hostname = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if (!hostname) return SSL_TLSEXT_ERR_NOACK;
    
    CertManager* cert_manager = static_cast<CertManager*>(arg);
    return cert_manager->select_certificate(ssl, hostname);
}

int CertManager::select_certificate(SSL *ssl, const char *hostname) {
    if (strcmp(hostname, "apm.papegames.com") == 0) {
        if (!SSL_use_certificate(ssl, siteCert) ||
            !SSL_use_PrivateKey(ssl, siteKey)) {
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }
    } else if (strcmp(hostname, "x6cn-clickhouse.nuanpaper.com") == 0) {
        if (!SSL_use_certificate(ssl, siteCert2) ||
            !SSL_use_PrivateKey(ssl, siteKey2)) {
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }
    } else {
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }
    return SSL_TLSEXT_ERR_OK;
}

EVP_PKEY* CertManager::generateRSAKey(int bits) {
    EVP_PKEY* key = EVP_PKEY_new();
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    
    if (!ctx || !key) {
        if (key) EVP_PKEY_free(key);
        if (ctx) EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0 ||
        EVP_PKEY_keygen(ctx, &key) <= 0) {
        EVP_PKEY_free(key);
        EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }

    EVP_PKEY_CTX_free(ctx);
    return key;
}

bool CertManager::addX509Extension(X509* cert, X509V3_CTX* ctx, int nid, const char* value) {
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, ctx, nid, value);
    if (!ext) return false;
    
    bool result = X509_add_ext(cert, ext, -1) == 1;
    X509_EXTENSION_free(ext);
    return result;
}

X509* CertManager::generateRootCertificate() {
    // 生成根证书密钥对
    rootKey = generateRSAKey();
    if (!rootKey) {
        SSLUtils::printOpenSSLError("Failed to create root key");
        return nullptr;
    }

    X509* cert = X509_new();
    if (!cert) return nullptr;

    // 设置版本(V3)
    X509_set_version(cert, 2);

    // 设置序列号
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);

    // 设置有效期
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 315360000L);  // 10年

    // 设置公钥
    X509_set_pubkey(cert, rootKey);

    // 设置证书主体和颁发者信息
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"CN", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)"SpinningMomo Proxy CA", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"SpinningMomo Root CA", -1, -1, 0);

    X509_set_issuer_name(cert, name);

    // 添加X509v3扩展
    X509V3_CTX v3ctx;
    X509V3_set_ctx_nodb(&v3ctx);
    X509V3_set_ctx(&v3ctx, cert, cert, nullptr, nullptr, 0);

    addX509Extension(cert, &v3ctx, NID_basic_constraints, "critical,CA:TRUE");
    addX509Extension(cert, &v3ctx, NID_key_usage, "critical,keyCertSign,cRLSign");
    addX509Extension(cert, &v3ctx, NID_subject_key_identifier, "hash");

    // 自签名
    if (!X509_sign(cert, rootKey, EVP_sha256())) {
        X509_free(cert);
        return nullptr;
    }

    return cert;
}

X509* CertManager::generateSiteCertificate(const char* domain, EVP_PKEY* key) {
    X509* cert = X509_new();
    if (!cert) return nullptr;

    // 设置版本(V3)
    X509_set_version(cert, 2);

    // 设置序列号（使用时间戳）
    ASN1_INTEGER_set(X509_get_serialNumber(cert), time(nullptr));

    // 设置有效期
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 315360000L);  // 10年

    // 设置公钥
    X509_set_pubkey(cert, key);

    // 设置证书主体信息
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"CN", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)"SpinningMomo Proxy", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)domain, -1, -1, 0);

    // 设置颁发者信息（使用根证书的信息）
    X509_set_issuer_name(cert, X509_get_subject_name(rootCA));

    // 添加X509v3扩展
    X509V3_CTX v3ctx;
    X509V3_set_ctx_nodb(&v3ctx);
    X509V3_set_ctx(&v3ctx, rootCA, cert, nullptr, nullptr, 0);

    std::string san = std::string("DNS:") + domain;
    addX509Extension(cert, &v3ctx, NID_subject_alt_name, san.c_str());
    addX509Extension(cert, &v3ctx, NID_ext_key_usage, "serverAuth");
    addX509Extension(cert, &v3ctx, NID_authority_key_identifier, "keyid:always,issuer");

    // 使用根证书私钥签名
    if (!X509_sign(cert, rootKey, EVP_sha256())) {
        X509_free(cert);
        return nullptr;
    }

    return cert;
}

bool CertManager::loadOrCreateCA() {
    // 尝试加载现有的CA证书
    FILE* fp = nullptr;
    if (fopen_s(&fp, CA_CERT, "r") == 0 && fp != nullptr) {
        rootCA = PEM_read_X509(fp, nullptr, nullptr, nullptr);
        fclose(fp);
        
        if (fopen_s(&fp, CA_KEY, "r") == 0 && fp != nullptr) {
            rootKey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
            fclose(fp);
        }
        
        if (rootCA && rootKey) return true;
    }

    // 如果加载失败，创建新的CA证书
    rootCA = generateRootCertificate();
    if (!rootCA) return false;

    // 保存CA证书和私钥
    if (fopen_s(&fp, CA_CERT, "w") == 0 && fp != nullptr) {
        PEM_write_X509(fp, rootCA);
        fclose(fp);
    }
    
    if (fopen_s(&fp, CA_KEY, "w") == 0 && fp != nullptr) {
        PEM_write_PrivateKey(fp, rootKey, nullptr, nullptr, 0, nullptr, nullptr);
        fclose(fp);
    }

    return true;
}

bool CertManager::loadOrCreateSiteCerts() {
    // 加载或创建第一个站点证书
    if (!loadOrCreateSiteCert("apm.papegames.com", siteCert, siteKey)) {
        return false;
    }
    // 加载或创建第二个站点证书
    if (!loadOrCreateSiteCert("x6cn-clickhouse.nuanpaper.com", siteCert2, siteKey2)) {
        return false;
    }
    return true;
}

bool CertManager::loadOrCreateSiteCert(const char* domain, X509*& cert, EVP_PKEY*& key) {
    std::string cert_path = std::string(CERT_DIR) + "/" + domain + "-cert.pem";
    std::string key_path = std::string(CERT_DIR) + "/" + domain + "-key.pem";

    // 尝试加载现有的站点证书
    FILE* fp = nullptr;
    if (fopen_s(&fp, cert_path.c_str(), "r") == 0 && fp != nullptr) {
        cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
        fclose(fp);
        
        if (fopen_s(&fp, key_path.c_str(), "r") == 0 && fp != nullptr) {
            key = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
            fclose(fp);
        }
        
        if (cert && key) return true;
    }

    // 如果加载失败，创建新的站点证书
    key = generateRSAKey();
    if (!key) {
        SSLUtils::printOpenSSLError("Failed to create site key");
        return false;
    }

    cert = generateSiteCertificate(domain, key);
    if (!cert) return false;

    // 保存站点证书和私钥
    if (fopen_s(&fp, cert_path.c_str(), "w") == 0 && fp != nullptr) {
        PEM_write_X509(fp, cert);
        fclose(fp);
    }
    
    if (fopen_s(&fp, key_path.c_str(), "w") == 0 && fp != nullptr) {
        PEM_write_PrivateKey(fp, key, nullptr, nullptr, 0, nullptr, nullptr);
        fclose(fp);
    }

    return true;
}

bool CertManager::installRootCertificate() {
    // 使用Windows命令行工具安装证书
    std::string cmd = "certutil -addstore -user Root " + std::string(CA_CERT);
    int result = system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Failed to install root certificate" << std::endl;
        return false;
    }
    return true;
} 