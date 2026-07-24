#pragma once

#include "core/state/app_state.hpp"
#include "features/settings/types.hpp"

namespace Features::Settings {

// 启动期仅依赖的最小设置子集。
// 用于在完整 AppState 初始化之前，先决定提权策略和初始日志级别。
struct StartupSettings {
  bool always_run_as_admin = true;
  std::optional<std::string> logger_level;
};

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

auto get_settings(Core::State::AppState& app_state) -> Types::GetSettingsResult;

auto update_settings(Core::State::AppState& app_state, const Types::UpdateSettingsParams& params)
    -> std::expected<Types::UpdateSettingsResult, std::string>;

auto patch_settings(Core::State::AppState& app_state, const Types::PatchSettingsParams& params)
    -> std::expected<Types::PatchSettingsResult, std::string>;

// 发布 settings 变更事件（new_settings 使用 app_state.settings->raw）
auto notify_settings_changed(Core::State::AppState& app_state,
                             const Types::AppSettings& old_settings,
                             std::string_view change_description) -> void;

auto get_settings_path() -> std::expected<std::filesystem::path, std::string>;

auto save_settings_to_file(const std::filesystem::path& settings_path,
                           const Types::AppSettings& config) -> std::expected<void, std::string>;

// 判断当前配置是否需要显示首次引导页
auto should_show_onboarding(const Types::AppSettings& settings) -> bool;

// 轻量级预读取：仅解析启动早期需要的少量字段。
// 设计目标：
// 1. 避免为了提权判断和早期日志初始化而拉起完整设置模块；
// 2. 即使 settings.json 缺失、损坏或字段不完整，也能稳定回退到默认值继续启动。
auto load_startup_settings() noexcept -> StartupSettings;

}  // namespace Features::Settings
