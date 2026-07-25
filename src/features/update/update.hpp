#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/state/app_state.hpp"
#include "features/update/state.hpp"
#include "features/update/types.hpp"

namespace features::update {

// 初始化Update模块
auto initialize(core::AppState& app_state) -> std::expected<void, std::string>;

// 启动时自动更新流程（按 settings 决定是否检查/下载/准备退出更新）
auto schedule_startup_auto_update_check(core::AppState& app_state) -> void;

// 检查更新
auto check_for_update(core::AppState& app_state)
    -> asio::awaitable<std::expected<CheckUpdateResult, std::string>>;

// 启动后台下载更新任务
auto start_download_update_task(core::AppState& app_state, bool prepare_install_on_exit = false)
    -> asio::awaitable<std::expected<StartDownloadUpdateResult, std::string>>;

// 安装更新
auto install_update(core::AppState& app_state, const InstallUpdateParams& params)
    -> std::expected<InstallUpdateResult, std::string>;

// 执行待处理的更新
auto execute_pending_update(core::AppState& app_state) -> void;

}  // namespace features::update
