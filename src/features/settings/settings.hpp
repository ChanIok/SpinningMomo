#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/settings/types.hpp"

namespace features::settings {

// 启动期仅依赖的最小设置子集。
// 用于在完整 AppState 初始化之前，先决定提权策略和初始日志级别。
struct StartupSettings {
  bool always_run_as_admin = true;
  std::optional<std::string> logger_level;
};

auto initialize(core::AppState& app_state) -> std::expected<void, std::string>;

auto get_settings(core::AppState& app_state) -> GetSettingsResult;

auto update_settings(core::AppState& app_state, const UpdateSettingsParams& params)
    -> std::expected<UpdateSettingsResult, std::string>;

auto patch_settings(core::AppState& app_state, const PatchSettingsParams& params)
    -> std::expected<PatchSettingsResult, std::string>;

// 发布 settings 变更事件（new_settings 使用 app_state.settings->raw）
auto notify_settings_changed(core::AppState& app_state, const AppSettings& old_settings,
                             std::string_view change_description) -> void;

auto get_settings_path() -> std::expected<std::filesystem::path, std::string>;

auto save_settings_to_file(const std::filesystem::path& settings_path, const AppSettings& config)
    -> std::expected<void, std::string>;

// 判断当前配置是否需要显示首次引导页
auto should_show_onboarding(const AppSettings& settings) -> bool;

// 轻量级预读取：仅解析启动早期需要的少量字段。
// 设计目标：
// 1. 避免为了提权判断和早期日志初始化而拉起完整设置模块；
// 2. 即使 settings.json 缺失、损坏或字段不完整，也能稳定回退到默认值继续启动。
auto load_startup_settings() noexcept -> StartupSettings;

}  // namespace features::settings
