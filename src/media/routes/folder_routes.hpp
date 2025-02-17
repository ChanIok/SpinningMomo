#pragma once

#include <uwebsockets/App.h>
#include <spdlog/spdlog.h>
#include "media/utils/logger.hpp"
#include "media/utils/response.hpp"
#include "media/utils/request.hpp"
#include "media/services/folder/folder_monitor_service.hpp"
#include "media/services/folder/folder_service.hpp"

/**
 * @brief 注册文件夹监控相关的所有路由
 * @param app uWebSockets应用实例
 */
inline void register_folder_routes(uWS::App& app) {
    auto& monitor_service = FolderMonitorService::get_instance();
    auto& folder_service = FolderService::get_instance();

    // GET /api/folders - 获取所有监控文件夹及其状态
    app.get("/api/folders", [&](auto* res, auto* req) {
        try {
            auto folders = monitor_service.get_all_folder_status();
            Response::Success(res, folders);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get folders: {}", e.what());
            Response::Error(res, "获取文件夹列表失败");
        }
    });

    // POST /api/folders - 添加监控文件夹
    app.post("/api/folders", [&](auto* res, auto* req) {
        struct Context {
            uWS::HttpResponse<false>* response;
            FolderMonitorService& service;
        };
        
        auto ctx = std::make_shared<Context>(Context{res, monitor_service});
        
        res->onAborted([ctx]() {
            spdlog::warn("Request aborted while adding folder");
        });
        
        res->onData([ctx](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                std::string path = json["path"].get<std::string>();
                
                if (ctx->service.add_folder(path)) {
                    Response::SuccessMessage(ctx->response, "文件夹添加成功");
                } else {
                    Response::Error(ctx->response, "添加文件夹失败");
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to add folder: {}", e.what());
                Response::Error(ctx->response, std::string("添加文件夹失败: ") + e.what());
            }
        });
    });

    // DELETE /api/folders/:path - 删除监控文件夹
    app.del("/api/folders/:path", [&](auto* res, auto* req) {
        try {
            std::string path = Request::GetPathParam(req, 0);
            if (monitor_service.remove_folder(path)) {
                Response::SuccessMessage(res, "文件夹删除成功");
            } else {
                Response::Error(res, "删除文件夹失败");
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to remove folder: {}", e.what());
            Response::Error(res, "删除文件夹失败");
        }
    });

    // GET /api/folders/:path/status - 获取特定文件夹的处理状态
    app.get("/api/folders/:path/status", [&](auto* res, auto* req) {
        try {
            std::string path = Request::GetPathParam(req, 0);
            auto status = monitor_service.get_folder_status(path);
            Response::Success(res, status);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get folder status: {}", e.what());
            Response::Error(res, "获取文件夹状态失败");
        }
    });

    // POST /api/folders/:path/reprocess - 重新处理文件夹
    app.post("/api/folders/:path/reprocess", [&](auto* res, auto* req) {
        try {
            std::string path = Request::GetPathParam(req, 0);
            if (monitor_service.reprocess_folder(path)) {
                Response::SuccessMessage(res, "开始重新处理文件夹");
            } else {
                Response::Error(res, "重新处理文件夹失败");
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to reprocess folder: {}", e.what());
            Response::Error(res, "重新处理文件夹失败");
        }
    });

    // POST /api/folders/select - 选择文件夹（调用系统文件夹选择对话框）
    app.post("/api/folders/select", [&](auto* res, auto* req) {
        try {
            auto selected_path = folder_service.show_folder_dialog();
            if (!selected_path) {
                Response::Error(res, "未选择文件夹");
                return;
            }

            bool is_accessible = folder_service.validate_folder_access(*selected_path);
            nlohmann::json result = {
                {"path", *selected_path},
                {"isAccessible", is_accessible}
            };
            
            Response::Success(res, result);
        } catch (const std::exception& e) {
            spdlog::error("Failed to select folder: {}", e.what());
            Response::Error(res, "选择文件夹失败");
        }
    });
} 