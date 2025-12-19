module;

module Features.Settings;

import std;
import Core.State;
import Core.Events;
import Features.Registry;
import Features.Settings.Events;
import Features.Settings.Types;
import Features.Settings.State;
import Features.Settings.Compute;
import Features.Settings.Migration;
import Features.Settings.Menu;
import Utils.Path;
import Utils.Logger;
import <rfl/json.hpp>;

namespace Features::Settings {

auto get_settings_path() -> std::expected<std::filesystem::path, std::string> {
  auto dir_result = Utils::Path::GetExecutableDirectory();
  if (!dir_result) {
    return std::unexpected("Failed to get executable directory: " + dir_result.error());
  }

  return dir_result.value() / "settings.json";
}

// Migration专用：迁移settings文件到指定版本
auto migrate_settings_file(const std::filesystem::path& file_path, int target_version)
    -> std::expected<void, std::string> {
  try {
    if (!std::filesystem::exists(file_path)) {
      return std::unexpected("Settings file does not exist: " + file_path.string());
    }

    // 读取原始JSON
    std::ifstream file(file_path);
    if (!file) {
      return std::unexpected("Failed to open settings file: " + file_path.string());
    }

    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // 使用rfl::Generic解析JSON以获取版本号
    auto generic_result = rfl::json::read<rfl::Generic::Object>(json_str);
    if (!generic_result) {
      return std::unexpected("Failed to parse settings as generic JSON: " +
                             generic_result.error().what());
    }

    auto generic_settings = generic_result.value();

    // 提取当前版本号
    int current_version = 1;  // 默认版本
    auto version_result = generic_settings.get("version").and_then(rfl::to_int);
    if (version_result) {
      current_version = version_result.value();
    }

    // 如果已经是目标版本，无需迁移
    if (current_version >= target_version) {
      Logger().info("Settings already at version {}, no migration needed", current_version);
      return {};
    }

    Logger().info("Migrating settings from version {} to {}", current_version, target_version);

    // 执行迁移
    auto migration_result = Migration::migrate_settings(generic_settings, current_version);
    if (!migration_result) {
      return std::unexpected("Settings migration failed: " + migration_result.error());
    }

    // 转换为AppSettings以验证结构
    auto app_settings_result = rfl::from_generic<Types::AppSettings>(migration_result.value());
    if (!app_settings_result) {
      return std::unexpected("Failed to convert migrated generic JSON to AppSettings: " +
                             app_settings_result.error().what());
    }

    // 保存迁移后的设置
    auto save_result = save_settings_to_file(file_path, app_settings_result.value());
    if (!save_result) {
      return std::unexpected("Failed to save migrated settings: " + save_result.error());
    }

    Logger().info("Settings migration completed successfully");
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Error during settings migration: " + std::string(e.what()));
  }
}

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    auto settings_path = get_settings_path();
    if (!settings_path) {
      return std::unexpected(settings_path.error());
    }

    // 情况1: 文件不存在 → 创建最新默认配置
    if (!std::filesystem::exists(settings_path.value())) {
      Logger().info("Settings file not found, creating default configuration");

      auto default_state = State::create_default_settings_state();

      auto json_str = rfl::json::write(default_state.raw, rfl::json::pretty);
      std::ofstream file(settings_path.value());
      if (!file) {
        return std::unexpected("Failed to create settings file");
      }
      file << json_str;

      // 计算预设并初始化内存状态
      *app_state.settings = default_state;
      Compute::trigger_compute(app_state);
      app_state.settings->is_initialized = true;

      Logger().info("Default settings created successfully");
      return {};
    }

    // 情况2: 文件存在 → 直接读取（Migration已保证版本正确）
    std::ifstream file(settings_path.value());
    if (!file) {
      return std::unexpected("Failed to open settings file");
    }

    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    auto config_result = rfl::json::read<Types::AppSettings, rfl::DefaultIfMissing>(json_str);
    if (!config_result) {
      return std::unexpected("Failed to parse settings: " + config_result.error().what());
    }

    auto config = config_result.value();

    // 创建完整状态
    State::SettingsState state;
    state.raw = config;

    // 先设置到app_state，然后计算预设
    *app_state.settings = state;
    Compute::trigger_compute(app_state);
    app_state.settings->is_initialized = true;

    Logger().info("Settings loaded successfully (version {})", config.version);
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

    auto result = rfl::json::read<Types::AppSettings, rfl::DefaultIfMissing>(json_str);
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
    Types::AppSettings old_settings = app_state.settings->raw;

    // 更新配置
    app_state.settings->raw = params;

    // 重新计算预设状态
    Compute::trigger_compute(app_state);

    // 保存到文件
    auto save_result = save_settings_to_file(settings_path.value(), params);
    if (!save_result) {
      // 回滚状态
      app_state.settings->raw = old_settings;
      Compute::trigger_compute(app_state);
      return std::unexpected(save_result.error());
    }

    // 发送设置变更事件
    Types::SettingsChangeData change_data{.old_settings = old_settings,
                                          .new_settings = app_state.settings->raw,
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