#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <uwebsockets/App.h>

/**
 * @brief HTTP响应工具类
 */
class Response {
public:
    /**
     * @brief 发送带数据的成功响应
     * @param res 响应对象
     * @param data 响应数据
     */
    template<typename T>
    static void Success(uWS::HttpResponse<false>* res, const T& data) {
        nlohmann::json response = {
            {"success", true},
            {"data", data}
        };
        SendJson(res, 200, response);
    }

    /**
     * @brief 发送只带消息的成功响应
     * @param res 响应对象
     * @param message 成功信息
     */
    static void SuccessMessage(uWS::HttpResponse<false>* res, const std::string& message) {
        nlohmann::json response = {
            {"success", true},
            {"data", {
                {"message", message}
            }}
        };
        SendJson(res, 200, response);
    }

    /**
     * @brief 发送错误响应
     * @param res 响应对象
     * @param message 错误信息
     * @param status HTTP状态码
     */
    static void Error(uWS::HttpResponse<false>* res, const std::string& message, int status = 400) {
        nlohmann::json response = {
            {"success", false},
            {"data", {
                {"message", message}
            }}
        };
        SendJson(res, status, response);
    }

    /**
     * @brief 发送成功的空响应（替代 204 No Content）
     * @param res 响应对象
     */
    static void NoContent(uWS::HttpResponse<false>* res) {
        SuccessMessage(res, "操作成功");
    }

private:
    /**
     * @brief 发送JSON响应
     * @param res 响应对象
     * @param status HTTP状态码
     * @param json JSON数据
     */
    static void SendJson(uWS::HttpResponse<false>* res, int status, const nlohmann::json& json) {
        res->writeHeader("Content-Type", "application/json");
        res->writeStatus(std::to_string(status) + " " + GetStatusText(status));
        std::string body = json.dump();
        res->end(body);
    }

    /**
     * @brief 获取HTTP状态码对应的文本描述
     * @param status HTTP状态码
     * @return 状态码文本描述
     */
    static std::string GetStatusText(int status) {
        switch (status) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "Unknown";
        }
    }
};