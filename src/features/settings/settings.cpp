module;

#include <rfl.hpp>
#include <rfl/json.hpp>

module Features.Settings;

import std;
import Core.State;
import Core.Events;
import Features.Settings.Events;
import Features.Settings.Types;
import Features.Settings.State;
import Features.Settings.Compute;
import Features.Settings.Migration;
import Utils.Path;
import Utils.Logger;

namespace Features::Settings {

auto get_settings_path() -> std::expected<std::filesystem::path, std::string> {
  auto dir_result = Utils::Path::GetExecutableDirectory();
  if (!dir_result) {
    return std::unexpected("Failed to get executable directory: " + dir_result.error());
  }

  return dir_result.value() / "settings.json";
}

// 专用于初始化的设置读取函数，使用rfl::Generic处理版本迁移
auto get_settings_for_initialization() -> std::expected<Types::AppSettings, std::string> {
  try {
    auto settings_path = get_settings_path();
    if (!settings_path) {
      return std::unexpected(settings_path.error());
    }

    if (!std::filesystem::exists(settings_path.value())) {
      return std::unexpected("Settings file does not exist");
    }

    // 读取原始JSON字符串
    std::ifstream file(settings_path.value());
    if (!file) {
      return std::unexpected("Failed to open settings file");
    }

    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // 使用rfl::Generic解析JSON以获取版本号
    auto generic_result = rfl::json::read<rfl::Generic::Object>(json_str);
    if (!generic_result) {
      return std::unexpected("Failed to parse settings as generic JSON: " +
                             generic_result.error().what());
    }

    auto generic_settings = generic_result.value();

    // 尝试提取版本号
    int version = 1;  // 默认版本
    auto version_result = generic_settings.get("version").and_then(rfl::to_int);
    if (version_result) {
      version = version_result.value();
    }

    // 如果版本低于当前版本，执行迁移
    if (version < Types::CURRENT_SETTINGS_VERSION) {
      // 执行迁移，传入原始的generic_settings
      auto migration_result = Migration::migrate_settings(generic_settings, version);
      if (!migration_result) {
        return std::unexpected("Settings migration failed: " + migration_result.error());
      }

      // 使用迁移后的generic对象转换为AppSettings
      auto app_settings_result = rfl::from_generic<Types::AppSettings>(migration_result.value());
      if (!app_settings_result) {
        return std::unexpected("Failed to convert migrated generic JSON to AppSettings: " +
                               app_settings_result.error().what());
      }

      auto migrated_settings = app_settings_result.value();

      // 自动保存迁移后的设置到文件
      auto save_result = save_settings_to_file(settings_path.value(), migrated_settings);
      if (!save_result) {
        return std::unexpected("Failed to save migrated settings: " + save_result.error());
      }

      return migrated_settings;
    }

    // 如果版本已经是最新版，直接转换为AppSettings
    auto app_settings_result =
        rfl::from_generic<Types::AppSettings, rfl::DefaultIfMissing>(generic_settings);
    if (!app_settings_result) {
      return std::unexpected("Failed to convert generic JSON to AppSettings: " +
                             app_settings_result.error().what());
    }

    return app_settings_result.value();
  } catch (const std::exception& e) {
    return std::unexpected("Failed to read settings: " + std::string(e.what()));
  }
}

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    auto settings_path = get_settings_path();
    if (!settings_path) {
      return std::unexpected(settings_path.error());
    }

    // 如果文件不存在，创建默认配置
    if (!std::filesystem::exists(settings_path.value())) {
      auto default_state = State::create_default_settings_state();

      auto json_str = rfl::json::write(default_state.config, rfl::json::pretty);
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
      // 从文件加载配置（使用专为初始化设计的函数，内部已处理迁移）
      auto config_result = get_settings_for_initialization();
      if (!config_result) {
        return std::unexpected(config_result.error());
      }

      auto config = config_result.value();

      // 创建完整状态
      State::SettingsState state;
      state.config = config;

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
    auto save_result = save_settings_to_file(settings_path.value(), params);
    if (!save_result) {
      // 回滚状态
      app_state.settings->config = old_settings;
      Compute::update_computed_state(app_state);
      return std::unexpected(save_result.error());
    }

    // 发送设置变更事件
    Types::SettingsChangeData change_data{.old_settings = old_settings,
                                          .new_settings = app_state.settings->config,
                                          .change_description = "Settings updated via RPC"};

    Core::Events::post(*app_state.events,
                       Features::Settings::Events::SettingsChangeEvent{change_data});

    Types::UpdateSettingsResult result;
    result.success = true;
    result.message = "Settings updated successfully";

    return result;
  } catch (const std::exception& e) {
    return std::unexpected("Failed to save settings: " + std::string(e.what()));
  }
}

auto save_settings_to_file(const std::filesystem::path& settings_path,
                           const Types::AppSettings& config) -> std::expected<void, std::string> {
  try {
    // 序列化配置到格式化的 JSON
    auto json_str = rfl::json::write(config, rfl::json::pretty);

    // 写入文件
    std::ofstream file(settings_path);
    if (!file) {
      return std::unexpected("Failed to open settings file for writing");
    }

    file << json_str;
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to save settings: " + std::string(e.what()));
  }
}

}  // namespace Features::Settings