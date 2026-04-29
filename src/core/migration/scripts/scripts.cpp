module;

module Core.Migration.Scripts;

import std;
import Core.State;
import Core.Database;
import Core.Migration.Schema;
import Features.Settings;
import Features.Settings.Types;
import Utils.Logger;
import <rfl/json.hpp>;

namespace Core::Migration::Scripts {

// 执行 SQL Schema 迁移的辅助函数
template <typename SchemaModule>
auto execute_sql_schema(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  for (const auto& sql : SchemaModule::statements) {
    auto result = Core::Database::execute(*app_state.database, std::string(sql));
    if (!result) {
      return std::unexpected(std::format("SQL execution failed: {}", result.error()));
    }
  }
  return {};
}

// Migration: 2.0.0.0 - Initialize database schema
auto migrate_v2_0_0_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  Logger().info("Executing migration to 2.0.0.0: Initialize database schema");

  // 首次启动，初始化数据库Schema
  // Settings会在后续initialize时自动创建最新配置
  auto result = execute_sql_schema<Core::Migration::Schema::V001>(app_state);

  if (!result) {
    return std::unexpected("Failed to initialize database schema: " + result.error());
  }

  Logger().info("Database schema initialized successfully");
  return {};
}

auto migrate_v2_0_1_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  Logger().info("Executing migration to 2.0.1.0: Add gallery watch root recovery state");

  auto result = execute_sql_schema<Core::Migration::Schema::V002>(app_state);
  if (!result) {
    return std::unexpected("Failed to add watch root recovery state schema: " + result.error());
  }

  return {};
}

auto migrate_v2_0_2_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  Logger().info("Executing migration to 2.0.2.0: Update version check URL");

  auto settings_path_result = Features::Settings::get_settings_path();
  if (!settings_path_result) {
    return std::unexpected("Failed to get settings path: " + settings_path_result.error());
  }

  const auto& settings_path = settings_path_result.value();
  if (!std::filesystem::exists(settings_path)) {
    Logger().info("Settings file not found, skip version URL migration");
    return {};
  }

  std::ifstream file(settings_path);
  if (!file) {
    return std::unexpected("Failed to open settings file: " + settings_path.string());
  }

  std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  auto settings_result =
      rfl::json::read<Features::Settings::Types::AppSettings, rfl::DefaultIfMissing>(json_str);
  if (!settings_result) {
    return std::unexpected("Failed to parse settings: " + settings_result.error().what());
  }

  auto settings = settings_result.value();
  settings.update.version_url = "https://spin.infinitymomo.com/version.txt";

  auto save_result = Features::Settings::save_settings_to_file(settings_path, settings);
  if (!save_result) {
    return std::unexpected("Failed to save settings: " + save_result.error());
  }

  Logger().info("Settings version URL migrated successfully");
  return {};
}

auto migrate_v2_0_8_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  Logger().info("Executing migration to 2.0.8.0: Add nuan5 extended Infinity Nikki columns");

  auto result = execute_sql_schema<Core::Migration::Schema::V003>(app_state);
  if (!result) {
    return std::unexpected("Failed to add nuan5 Infinity Nikki columns: " + result.error());
  }
  return {};
}

auto migrate_v2_0_9_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  Logger().info("Executing migration to 2.0.9.0: Rebuild Infinity Nikki user record as key-value");

  auto result = execute_sql_schema<Core::Migration::Schema::V004>(app_state);
  if (!result) {
    return std::unexpected("Failed to rebuild Infinity Nikki user record schema: " +
                           result.error());
  }
  return {};
}

auto migrate_v2_0_10_0(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  Logger().info("Executing migration to 2.0.10.0: Set update download sources");

  auto settings_path_result = Features::Settings::get_settings_path();
  if (!settings_path_result) {
    return std::unexpected("Failed to get settings path: " + settings_path_result.error());
  }

  const auto& settings_path = settings_path_result.value();
  if (!std::filesystem::exists(settings_path)) {
    Logger().info("Settings file not found, skip download source migration");
    return {};
  }

  std::ifstream file(settings_path);
  if (!file) {
    return std::unexpected("Failed to open settings file: " + settings_path.string());
  }

  std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  auto settings_result =
      rfl::json::read<Features::Settings::Types::AppSettings, rfl::DefaultIfMissing>(json_str);
  if (!settings_result) {
    return std::unexpected("Failed to parse settings: " + settings_result.error().what());
  }

  auto settings = settings_result.value();
  settings.update.download_sources = {
      Features::Settings::Types::AppSettings::Update::DownloadSource{
          "CNB", "https://cnb.cool/infinitymomo/SpinningMomo/-/releases/download/v{0}/{1}"},
      Features::Settings::Types::AppSettings::Update::DownloadSource{
          "Mirror", "https://r2.infinitymomo.com/releases/v{0}/{1}"},
      Features::Settings::Types::AppSettings::Update::DownloadSource{
          "GitHub", "https://github.com/ChanIok/SpinningMomo/releases/download/v{0}/{1}"},
  };

  auto save_result = Features::Settings::save_settings_to_file(settings_path, settings);
  if (!save_result) {
    return std::unexpected("Failed to save settings: " + save_result.error());
  }

  Logger().info("Settings download sources set successfully");
  return {};
}

auto get_all_migrations() -> const std::vector<MigrationScript>& {
  static const std::vector<MigrationScript> migrations = {
      {"2.0.0.0", "Initialize database schema", true, migrate_v2_0_0_0},
      {"2.0.1.0", "Add gallery watch root recovery state", true, migrate_v2_0_1_0},
      {"2.0.2.0", "Update version check URL", false, migrate_v2_0_2_0},
      {"2.0.8.0", "Add nuan5 Infinity Nikki extract columns", true, migrate_v2_0_8_0},
      {"2.0.9.0", "Rebuild Infinity Nikki user record as key-value", true, migrate_v2_0_9_0},
      {"2.0.10.0", "Set update download sources", false, migrate_v2_0_10_0},

      // 未来版本的迁移脚本在此添加
      // {"2.0.2.0", "Add user preferences", migrate_v2_0_2_0},
  };
  return migrations;
}

}  // namespace Core::Migration::Scripts
