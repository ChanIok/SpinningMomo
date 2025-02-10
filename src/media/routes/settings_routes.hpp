#pragma once

#include <uwebsockets/App.h>
#include <spdlog/spdlog.h>
#include "media/utils/logger.hpp"
#include "media/utils/response.hpp"
#include "media/utils/request.hpp"
#include "core/settings/settings_manager.hpp"
#include "media/services/folder_service.hpp"

/**
 * @brief 注册设置相关的所有路由
 * @param app uWebSockets应用实例
 */
inline void register_settings_routes(uWS::App& app) {
    auto& settings_manager = SettingsManager::get_instance();
    auto& folder_service = FolderService::get_instance();

    // GET /api/settings - 获取所有设置
    app.get("/api/settings", [&](auto* res, auto* req) {
        try {
            auto settings = settings_manager.get_settings();
            Response::Success(res, settings);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get settings: {}", e.what());
            Response::Error(res, "Failed to get settings", 500);
        }
    });

    // PUT /api/settings - 更新所有设置
    app.put("/api/settings", [&](auto* res, auto* req) {
        struct Context {
            uWS::HttpResponse<false>* response;
            SettingsManager& manager;
        };
        
        auto ctx = std::make_shared<Context>(Context{res, settings_manager});
        
        res->onAborted([ctx]() {
            spdlog::warn("Request aborted while updating settings");
        });
        
        res->onData([ctx](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto settings = json.get<AppSettings>();
                
                if (ctx->manager.update_settings(settings)) {
                    Response::Success(ctx->response, settings);
                } else {
                    Response::Error(ctx->response, "Failed to update settings");
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to update settings: {}", e.what());
                Response::Error(ctx->response, "Failed to update settings");
            }
        });
    });

    // GET /api/settings/watched-folders - 获取监视文件夹列表
    app.get("/api/settings/watched-folders", [&](auto* res, auto* req) {
        try {
            auto folders = settings_manager.get_watched_folders();
            Response::Success(res, folders);
        } catch (const std::exception& e) {
            spdlog::error("Failed to get watched folders: {}", e.what());
            Response::Error(res, "Failed to get watched folders", 500);
        }
    });

    // POST /api/settings/watched-folders - 添加监视文件夹
    app.post("/api/settings/watched-folders", [&](auto* res, auto* req) {
        struct Context {
            uWS::HttpResponse<false>* response;
            SettingsManager& manager;
        };
        
        auto ctx = std::make_shared<Context>(Context{res, settings_manager});
        
        res->onAborted([ctx]() {
            spdlog::warn("Request aborted while adding watched folder");
        });
        
        res->onData([ctx](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto folder = json.get<WatchedFolder>();
                
                if (ctx->manager.add_watched_folder(folder)) {
                    Response::Success(ctx->response, folder);
                } else {
                    Response::Error(ctx->response, "Folder already exists");
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to add watched folder: {}", e.what());
                Response::Error(ctx->response, "Failed to add watched folder");
            }
        });
    });

    // DELETE /api/settings/watched-folders/:path - 删除监视文件夹
    app.del("/api/settings/watched-folders/:path", [&](auto* res, auto* req) {
        try {
            auto encoded_path = Request::GetPathParam(req, 0);
            spdlog::debug("Raw path parameter: {}", encoded_path);
            
            auto query = std::string("?path=") + std::string(encoded_path);
            auto path = std::string(uWS::getDecodedQueryValue("path", query));
            
            spdlog::debug("Removing watched folder, received path: {}", path);
            spdlog::debug("Current watched folders:");
            for (const auto& folder : settings_manager.get_watched_folders()) {
                spdlog::debug("- {}", folder.path);
            }
            
            if (settings_manager.remove_watched_folder(path)) {
                Response::SuccessMessage(res, "Folder removed successfully");
            } else {
                spdlog::debug("No matching folder found for path: {}", path);
                Response::Error(res, "Folder not found", 404);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to remove watched folder: {}", e.what());
            Response::Error(res, "Failed to remove watched folder");
        }
    });

    // PUT /api/settings/thumbnail - 更新缩略图设置
    app.put("/api/settings/thumbnail", [&](auto* res, auto* req) {
        res->onData([res, &settings_manager](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto settings = json.get<ThumbnailSettings>();
                
                if (settings_manager.update_thumbnail_settings(settings)) {
                    Response::Success(res, settings);
                } else {
                    Response::Error(res, "Failed to update thumbnail settings");
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to update thumbnail settings: {}", e.what());
                Response::Error(res, "Failed to update thumbnail settings");
            }
        });
    });

    // PUT /api/settings/interface - 更新界面设置
    app.put("/api/settings/interface", [&](auto* res, auto* req) {
        res->onData([res, &settings_manager](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto settings = json.get<InterfaceSettings>();
                
                if (settings_manager.update_interface_settings(settings)) {
                    Response::Success(res, settings);
                } else {
                    Response::Error(res, "Failed to update interface settings");
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to update interface settings: {}", e.what());
                Response::Error(res, "Failed to update interface settings");
            }
        });
    });

    // PUT /api/settings/performance - 更新性能设置
    app.put("/api/settings/performance", [&](auto* res, auto* req) {
        res->onData([res, &settings_manager](std::string_view body, bool last) {
            if (!last) return;
            
            try {
                auto json = Request::ParseJson(body);
                auto settings = json.get<PerformanceSettings>();
                
                if (settings_manager.update_performance_settings(settings)) {
                    Response::Success(res, settings);
                } else {
                    Response::Error(res, "Failed to update performance settings");
                }
            } catch (const std::exception& e) {
                spdlog::error("Failed to update performance settings: {}", e.what());
                Response::Error(res, "Failed to update performance settings");
            }
        });
    });

    // POST /api/settings/select-folder - 打开文件夹选择对话框
    app.post("/api/settings/select-folder", [&](auto* res, auto* req) {
        try {
            auto result = folder_service.show_folder_dialog();
            if (result) {
                nlohmann::json response = {
                    {"path", *result},
                    {"isAccessible", folder_service.validate_folder_access(*result)}
                };
                Response::Success(res, response);
            } else {
                Response::Error(res, "Failed to select folder");
            }
        } catch (const std::exception& e) {
            spdlog::error("Error in folder selection: {}", e.what());
            Response::Error(res, "Failed to select folder");
        }
    });
} 