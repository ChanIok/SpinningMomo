module;

#include <rfl.hpp>
#include <rfl/json.hpp>

module Features.Settings;

import std;
import Core.State;
import Core.Events;
import Features.Settings.Types;
import Features.Settings.State;
import Features.Settings.Compute;
import Types.Presets;
import Utils.Path;

namespace Features::Settings {

auto get_settings_path() -> std::expected<std::filesystem::path, std::string> {
  auto dir_result = Utils::Path::GetExecutableDirectory();
  if (!dir_result) {
    return std::unexpected("Failed to get executable directory: " + dir_result.error());
  }

  return dir_result.value() / "settings.json";
}

// === 业务逻辑函数 ===

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    auto settings_path = get_settings_path();
    if (!settings_path) {
      return std::unexpected(settings_path.error());
    }

    // 如果文件不存在，创建默认配置
    if (!std::filesystem::exists(settings_path.value())) {
      auto default_state = State::create_default_settings_state();

      auto json_str = rfl::json::write(default_state.config);
      std::ofstream file(settings_path.value());
      if (!file) {
        return std::unexpected("Failed to create settings file");
      }
      file << json_str;

      // 计算预设并初始化内存状态
      *app_state.settings = default_state;
      Compute::update_computed_state(app_state);
      app_state.settings->is_initialized = true;

    } else {
      // 从文件加载配置
      auto config_result = get_settings({});
      if (!config_result) {
        return std::unexpected(config_result.error());
      }

      // 创建完整状态
      State::SettingsState state;
      state.config = config_result.value();

      // 先设置到app_state，然后计算预设
      *app_state.settings = state;
      Compute::update_computed_state(app_state);
      app_state.settings->is_initialized = true;
    }

    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to initialize settings: " + std::string(e.what()));
  }
}

auto get_settings(const Types::GetSettingsParams& params)
    -> std::expected<Types::GetSettingsResult, std::string> {
  try {
    auto settings_path = get_settings_path();
    if (!settings_path) {
      return std::unexpected(settings_path.error());
    }

    if (!std::filesystem::exists(settings_path.value())) {
      return std::unexpected("Settings file does not exist");
    }

    std::ifstream file(settings_path.value());
    if (!file) {
      return std::unexpected("Failed to open settings file");
    }

    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    auto result = rfl::json::read<Types::AppSettings>(json_str);
    if (!result) {
      return std::unexpected("Failed to parse settings: " + result.error().what());
    }

    return result.value();
  } catch (const std::exception& e) {
    return std::unexpected("Failed to read settings: " + std::string(e.what()));
  }
}

auto update_settings(Core::State::AppState& app_state, const Types::UpdateSettingsParams& params)
    -> std::expected<Types::UpdateSettingsResult, std::string> {
  try {
    auto settings_path = get_settings_path();
    if (!settings_path) {
      return std::unexpected(settings_path.error());
    }

    // 确保 settings 已初始化
    if (!app_state.settings) {
      return std::unexpected("Settings not initialized");
    }

    // 保存旧设置用于事件通知
    Types::AppSettings old_settings = app_state.settings->config;

    // 更新配置
    app_state.settings->config = params;

    // 重新计算预设状态
    Compute::update_computed_state(app_state);

    // 保存到文件
    auto json_str = rfl::json::write(params);

    std::ofstream file(settings_path.value());
    if (!file) {
      // 回滚状态
      app_state.settings->config = old_settings;
      Compute::update_computed_state(app_state);
      return std::unexpected("Failed to open settings file for writing");
    }

    file << json_str;

    // 发送设置变更事件
    Types::SettingsChangeData change_data{.old_settings = old_settings,
                                          .new_settings = app_state.settings->config,
                                          .change_description = "Settings updated via RPC"};

    Core::Events::post_event(
        *app_state.event_bus,
        Core::Events::Event{Core::Events::EventType::ConfigChanged, change_data});

    Types::UpdateSettingsResult result;
    result.success = true;
    result.message = "Settings updated successfully";

    return result;
  } catch (const std::exception& e) {
    return std::unexpected("Failed to save settings: " + std::string(e.what()));
  }
}

}  // namespace Features::Settings