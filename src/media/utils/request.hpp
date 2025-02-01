#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <uwebsockets/App.h>

/**
 * @brief HTTP请求工具类
 */
class Request {
public:
    /**
     * @brief 从URL中获取路径参数
     * @param req 请求对象
     * @param index 参数索引
     * @return 参数值
     */
    static std::string GetPathParam(uWS::HttpRequest* req, size_t index) {
        return std::string(req->getParameter(index));
    }

    /**
     * @brief 获取查询参数
     * @param req 请求对象
     * @param name 参数名
     * @return 参数值（如果存在）
     */
    static std::optional<std::string> GetQueryParam(uWS::HttpRequest* req, const std::string& name) {
        auto query = req->getQuery();
        auto pos = query.find(name + "=");
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        
        auto value_start = pos + name.length() + 1;
        auto value_end = query.find('&', value_start);
        if (value_end == std::string::npos) {
            value_end = query.length();
        }
        
        return std::optional<std::string>(std::string(query.substr(value_start, value_end - value_start)));
    }

    /**
     * @brief 解析请求体为JSON
     * @param body 请求体字符串
     * @return JSON对象
     */
    static nlohmann::json ParseJson(std::string_view body) {
        try {
            return nlohmann::json::parse(body);
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error("Invalid JSON format");
        }
    }
}; 