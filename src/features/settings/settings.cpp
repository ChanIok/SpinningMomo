module;

#include <rfl.hpp>
#include <rfl/json.hpp>

module Features.Settings;

import std;
import Core.State;
import Core.Events;
import Features.Settings.Types;
import Utils.Path;

namespace Features::Settings {

auto get_settings_path() -> std::expected<std::filesystem::path, std::string> {
  auto dir_result = Utils::Path::GetExecutableDirectory();
  if (!dir_result) {
    return std::unexpected("Failed to get executable directory: " + dir_result.error());
  }

  return dir_result.value() / "settings.json";
}

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    auto settings_path = get_settings_path();
    if (!settings_path) {
      return std::unexpected(settings_path.error());
    }

    // 如果文件不存在，创建默认配置
    if (!std::filesystem::exists(settings_path.value())) {
      Types::GetSettingsResult default_settings;
      default_settings.window.title = "";
      default_settings.version = "1.0";

      auto json_str = rfl::json::write(default_settings);
      std::ofstream file(settings_path.value());
      if (!file) {
        return std::unexpected("Failed to create settings file");
      }
      file << json_str;
      
      // 初始化内存状态
      app_state.settings.window.title = "";
      app_state.settings.version = "1.0";
    } else {
      // 从文件加载到内存状态
      auto result = get_settings({});
      if (result) {
        app_state.settings.window.title = result.value().window.title;
        app_state.settings.version = result.value().version;
      }
    }

    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Initialization failed: " + std::string(e.what()));
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

    auto result = rfl::json::read<Types::GetSettingsResult>(json_str);
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

    // 保存旧设置用于事件通知
    Types::AppSettings old_settings = app_state.settings;

    // 更新内存中的全局状态
    app_state.settings.window = params.window;
    app_state.settings.version = "1.0";

    // 构造完整的设置对象并保存到文件
    Types::GetSettingsResult settings;
    settings.window = params.window;
    settings.version = "1.0";

    auto json_str = rfl::json::write(settings);

    std::ofstream file(settings_path.value());
    if (!file) {
      // 回滚内存状态
      app_state.settings = old_settings;
      return std::unexpected("Failed to open settings file for writing");
    }

    file << json_str;

    // 发送设置变更事件
    Types::SettingsChangeData change_data{
        .old_settings = old_settings,
        .new_settings = app_state.settings,
        .change_description = "Settings updated via RPC"};

    Core::Events::post_event(app_state.event_bus, 
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