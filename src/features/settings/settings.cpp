module;

module Features.Settings;

import std;
import Core.State;
import Core.Events;
import Features.Settings.Events;
import Features.Settings.Types;
import Features.Settings.State;
import Features.Settings.Compute;
import Features.Settings.Background;
import Utils.Path;
import Utils.Logger;
import Vendor.Windows;
import <rfl/json.hpp>;

namespace Features::Settings {

namespace Detail {

// 启动期只关心 app 下的极少数字段；
// 单独声明一个最小映射结构，避免在完整初始化前引入更多不必要的耦合。
struct StartupLoggerSettings {
  std::optional<std::string> level;
};

struct StartupAppSettings {
  bool always_run_as_admin = true;
  StartupLoggerSettings logger;
};

struct StartupSettingsFile {
  StartupAppSettings app;
};

}  // namespace Detail

auto detect_default_locale() -> std::string {
  constexpr Vendor::Windows::LANGID kPrimaryLanguageMask = 0x03ff;
  constexpr Vendor::Windows::LANGID kChinesePrimaryLanguage = 0x0004;

  auto language_id = Vendor::Windows::GetUserDefaultUILanguage();
  auto primary_language = static_cast<Vendor::Windows::LANGID>(language_id & kPrimaryLanguageMask);

  if (primary_language == kChinesePrimaryLanguage) {
    return "zh-CN";
  }

  return "en-US";
}

auto get_settings_path() -> std::expected<std::filesystem::path, std::string> {
  return Utils::Path::GetAppDataFilePath("settings.json");
}

auto should_show_onboarding(const Types::AppSettings& settings) -> bool {
  if (!settings.app.onboarding.completed) {
    return true;
  }

  return settings.app.onboarding.flow_version < Types::CURRENT_ONBOARDING_FLOW_VERSION;
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

      Types::AppSettings config;
      config.app.language.current = detect_default_locale();
      // 新安装用户首次启动应进入欢迎流程
      config.app.onboarding.completed = false;
      config.app.onboarding.flow_version = Types::CURRENT_ONBOARDING_FLOW_VERSION;
      config.extensions.infinity_nikki.enable = false;

      auto json_str = rfl::json::write(config, rfl::json::pretty);
      std::ofstream file(settings_path.value());
      if (!file) {
        return std::unexpected("Failed to create settings file");
      }
      file << json_str;

      // 计算预设并初始化内存状态
      app_state.settings->raw = std::move(config);
      Compute::trigger_compute(app_state);
      app_state.settings->is_initialized = true;
      Background::register_static_resolvers(app_state);

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

    app_state.settings->raw = std::move(config_result.value());
    Compute::trigger_compute(app_state);
    app_state.settings->is_initialized = true;
    Background::register_static_resolvers(app_state);

    Logger().info("Settings loaded successfully (version {})", app_state.settings->raw.version);
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to initialize settings: " + std::string(e.what()));
  }
}

auto get_settings(Core::State::AppState& app_state) -> Types::GetSettingsResult {
  std::scoped_lock lock(app_state.settings->mutation_mutex);
  return app_state.settings->raw;
}

auto notify_settings_changed(Core::State::AppState& app_state,
                             const Types::AppSettings& old_settings,
                             std::string_view change_description) -> void {
  Types::SettingsChangeData change_data{
      .old_settings = old_settings,
      .new_settings = app_state.settings->raw,
      .change_description = std::string(change_description),
  };
  Core::Events::post(app_state, Features::Settings::Events::SettingsChangeEvent{change_data});
}

auto merge_patch_object(rfl::Generic::Object& target, const rfl::Generic::Object& patch) -> void {
  for (const auto& [key, patch_value] : patch) {
    auto patch_object = patch_value.to_object();
    if (!patch_object) {
      target[key] = patch_value;
      continue;
    }

    auto target_object =
        target.get(key).and_then([](const rfl::Generic& value) { return value.to_object(); });
    if (target_object) {
      auto merged_child = target_object.value();
      merge_patch_object(merged_child, patch_object.value());
      target[key] = merged_child;
      continue;
    }

    target[key] = patch_value;
  }
}

auto apply_settings_and_persist_locked(Core::State::AppState& app_state,
                                       const Types::AppSettings& next_settings,
                                       std::string_view change_description)
    -> std::expected<Types::UpdateSettingsResult, std::string> {
  auto settings_path = get_settings_path();
  if (!settings_path) {
    return std::unexpected(settings_path.error());
  }

  Types::AppSettings old_settings = app_state.settings->raw;
  app_state.settings->raw = next_settings;
  Compute::trigger_compute(app_state);

  auto save_result = save_settings_to_file(settings_path.value(), app_state.settings->raw);
  if (!save_result) {
    app_state.settings->raw = old_settings;
    Compute::trigger_compute(app_state);
    return std::unexpected(save_result.error());
  }

  notify_settings_changed(app_state, old_settings, change_description);

  return Types::UpdateSettingsResult{
      .success = true,
      .message = "Settings updated successfully",
  };
}

auto update_settings(Core::State::AppState& app_state, const Types::UpdateSettingsParams& params)
    -> std::expected<Types::UpdateSettingsResult, std::string> {
  try {
    std::scoped_lock lock(app_state.settings->mutation_mutex);
    return apply_settings_and_persist_locked(app_state, params, "Settings updated via RPC");
  } catch (const std::exception& e) {
    return std::unexpected("Failed to save settings: " + std::string(e.what()));
  }
}

auto patch_settings(Core::State::AppState& app_state, const Types::PatchSettingsParams& params)
    -> std::expected<Types::PatchSettingsResult, std::string> {
  try {
    if (params.patch.empty()) {
      return Types::PatchSettingsResult{
          .success = true,
          .message = "No settings changes",
      };
    }

    std::scoped_lock lock(app_state.settings->mutation_mutex);

    auto merged_object =
        rfl::to_generic<rfl::SnakeCaseToCamelCase>(app_state.settings->raw).to_object().value();
    merge_patch_object(merged_object, params.patch);

    // 经 JSON 再反序列化，与「从文件读设置」同路径，避免 from_generic 对 double 字段
    // 不接受整数（如 100% → 1）导致的解析失败
    auto merged_json = rfl::json::write(merged_object);
    auto merged_settings_result =
        rfl::json::read<Types::AppSettings, rfl::DefaultIfMissing, rfl::SnakeCaseToCamelCase>(
            merged_json);
    if (!merged_settings_result) {
      return std::unexpected("Invalid settings patch: " + merged_settings_result.error().what());
    }

    return apply_settings_and_persist_locked(app_state, merged_settings_result.value(),
                                             "Settings patched via RPC");
  } catch (const std::exception& e) {
    return std::unexpected("Failed to patch settings: " + std::string(e.what()));
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

auto load_startup_settings() noexcept -> StartupSettings {
  StartupSettings startup_settings;

  try {
    // 启动早期允许 settings.json 不存在；此时直接使用默认值继续启动。
    auto settings_path = get_settings_path();
    if (!settings_path || !std::filesystem::exists(settings_path.value())) {
      return startup_settings;
    }

    std::ifstream file(settings_path.value());
    if (!file) {
      return startup_settings;
    }

    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // 这里只解析启动阶段真正需要的字段：
    // - app.always_run_as_admin
    // - app.logger.level
    // 任何解析失败都应静默回退到默认值，不能阻塞应用启动。
    auto startup_result =
        rfl::json::read<Detail::StartupSettingsFile, rfl::DefaultIfMissing>(json_str);
    if (!startup_result) {
      return startup_settings;
    }

    startup_settings.always_run_as_admin = startup_result->app.always_run_as_admin;
    if (startup_result->app.logger.level.has_value() &&
        !startup_result->app.logger.level->empty()) {
      startup_settings.logger_level = startup_result->app.logger.level;
    }

    return startup_settings;
  } catch (...) {
    // 启动早期不向上抛异常，统一退回默认行为。
    return startup_settings;
  }
}

}  // namespace Features::Settings
