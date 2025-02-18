#pragma once
#include <string>
#include <sstream>
#include <regex>

class HTTPParser {
public:
    // 解析HTTP头部
    static std::string parseHeaders(const std::string& data, bool isRequest);
    
    // 格式化HTTP请求
    static std::string formatRequest(const std::string& method, 
                                   const std::string& path,
                                   const std::string& headers);
    
    // 格式化HTTP响应
    static std::string formatResponse(int statusCode,
                                    const std::string& statusText,
                                    const std::string& headers);
}; 