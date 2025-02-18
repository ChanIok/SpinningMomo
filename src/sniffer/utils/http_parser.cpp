#include "http_parser.hpp"
#include <iomanip>

std::string HTTPParser::parseHeaders(const std::string& data, bool isRequest) {
    std::stringstream output;
    std::istringstream stream(data);
    std::string line;

    // 解析第一行
    std::getline(stream, line);
    if (line.empty()) return "";

    if (isRequest) {
        // 解析HTTP请求行（例如：GET /index.html HTTP/1.1）
        std::regex requestPattern("(GET|POST|PUT|DELETE|HEAD|OPTIONS|PATCH) (.*) HTTP/\\d\\.\\d");
        std::smatch matches;
        if (std::regex_search(line, matches, requestPattern)) {
            output << "\n=== Request ===\n";
            output << "Method: " << matches[1] << "\n";  // HTTP方法
            output << "Path: " << matches[2] << "\n";    // 请求路径
        }
    } else {
        // 解析HTTP响应行（例如：HTTP/1.1 200 OK）
        std::regex responsePattern("HTTP/\\d\\.\\d (\\d{3}) (.*)");
        std::smatch matches;
        if (std::regex_search(line, matches, responsePattern)) {
            output << "\n=== Response ===\n";
            output << "Status: " << matches[1] << " " << matches[2] << "\n";  // 状态码和描述
        }
    }

    // 解析HTTP头部字段
    while (std::getline(stream, line) && line != "\r") {
        if (line.empty() || line == "\r") break;
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // 移除前导空格
            value = value.substr(value.find_first_not_of(" "));
            // 移除末尾的\r
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }

            // 选择性输出重要的HTTP头部字段
            if (isRequest && (key == "Host" || key == "Content-Type" || key == "Content-Length")) {
                output << key << ": " << value << "\n";
            } else if (!isRequest && (key == "Content-Type" || key == "Content-Length" || key == "Server")) {
                output << key << ": " << value << "\n";
            }
        }
    }
    return output.str();
}

std::string HTTPParser::formatRequest(const std::string& method, 
                                    const std::string& path,
                                    const std::string& headers) {
    std::stringstream ss;
    ss << method << " " << path << " HTTP/1.1\r\n"
       << headers
       << "\r\n";
    return ss.str();
}

std::string HTTPParser::formatResponse(int statusCode,
                                     const std::string& statusText,
                                     const std::string& headers) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
       << headers
       << "\r\n";
    return ss.str();
} 