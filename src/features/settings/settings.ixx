module;

export module Features.Settings;

import std;
import Core.State;
import Features.Settings.Types;

namespace Features::Settings {

export auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string>;

export auto get_settings(const Types::GetSettingsParams& params)
    -> std::expected<Types::GetSettingsResult, std::string>;

export auto update_settings(Core::State::AppState& app_state,
                            const Types::UpdateSettingsParams& params)
    -> std::expected<Types::UpdateSettingsResult, std::string>;

export auto patch_settings(Core::State::AppState& app_state,
                           const Types::PatchSettingsParams& params)
    -> std::expected<Types::PatchSettingsResult, std::string>;

// 发布 settings 变更事件（new_settings 使用 app_state.settings->raw）
export auto notify_settings_changed(Core::State::AppState& app_state,
                                    const Types::AppSettings& old_settings,
                                    std::string_view change_description) -> void;

export auto get_settings_path() -> std::expected<std::filesystem::path, std::string>;

export auto save_settings_to_file(const std::filesystem::path& settings_path,
                                  const Types::AppSettings& config)
    -> std::expected<void, std::string>;

// Migration专用接口：迁移settings文件到指定版本
export auto migrate_settings_file(const std::filesystem::path& file_path, int target_version)
    -> std::expected<void, std::string>;

// 轻量级预读取：检查是否需要以管理员权限运行
// 此函数不依赖 AppState，可在应用初始化之前调用
export auto should_run_as_admin() noexcept -> bool;

}  // namespace Features::Settings
