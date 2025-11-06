module;

module Core.Migration;

import std;
import Core.State;
import Core.Migration.Scripts;
import Utils.Logger;
import Utils.Path;
import Vendor.Version;

namespace Core::Migration {

auto get_version_file_path() -> std::expected<std::filesystem::path, std::string> {
  // 存储在程序所在目录的 data 子目录下
  auto exe_dir = Utils::Path::GetExecutableDirectory();
  if (!exe_dir) {
    return std::unexpected("Failed to get executable directory: " + exe_dir.error());
  }

  auto data_dir = exe_dir.value() / "data";

  // 确保目录存在
  auto ensure_result = Utils::Path::EnsureDirectoryExists(data_dir);
  if (!ensure_result) {
    return std::unexpected("Failed to create data directory: " + ensure_result.error());
  }

  return data_dir / "app_version.txt";
}

auto get_last_version() -> std::expected<std::string, std::string> {
  auto path_result = get_version_file_path();
  if (!path_result) {
    return std::unexpected("Failed to get version file path: " + path_result.error());
  }

  auto path = path_result.value();

  // 首次启动，版本文件不存在
  if (!std::filesystem::exists(path)) {
    Logger().info("Version file not found, assuming first launch");
    return "0.0.0.0";
  }

  try {
    std::ifstream file(path);
    if (!file) {
      return std::unexpected("Failed to open version file: " + path.string());
    }

    std::string version;
    std::getline(file, version);

    if (version.empty()) {
      return std::unexpected("Version file is empty");
    }

    Logger().info("Last version: {}", version);
    return version;

  } catch (const std::exception& e) {
    return std::unexpected("Error reading version file: " + std::string(e.what()));
  }
}

auto save_current_version(const std::string& version) -> std::expected<void, std::string> {
  auto path_result = get_version_file_path();
  if (!path_result) {
    return std::unexpected("Failed to get version file path: " + path_result.error());
  }

  auto path = path_result.value();

  try {
    std::ofstream file(path);
    if (!file) {
      return std::unexpected("Failed to open version file for writing: " + path.string());
    }

    file << version;

    if (!file) {
      return std::unexpected("Failed to write version to file");
    }

    Logger().info("Version saved: {}", version);
    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Error saving version file: " + std::string(e.what()));
  }
}

// 简单的版本号比较（支持 x.y.z.w 格式，4段版本号）
auto compare_versions(const std::string& v1, const std::string& v2) -> int {
  auto parse_version = [](const std::string& v) -> std::vector<int> {
    std::vector<int> parts;
    std::istringstream ss(v);
    std::string part;

    while (std::getline(ss, part, '.')) {
      try {
        parts.push_back(std::stoi(part));
      } catch (const std::invalid_argument&) {
        parts.push_back(0);
      } catch (const std::out_of_range&) {
        parts.push_back(0);
      }
    }

    // 补齐到4位（与 version.hpp 的格式一致）
    while (parts.size() < 4) {
      parts.push_back(0);
    }

    return parts;
  };

  auto parts1 = parse_version(v1);
  auto parts2 = parse_version(v2);

  for (size_t i = 0; i < 4; ++i) {
    if (parts1[i] < parts2[i]) return -1;
    if (parts1[i] > parts2[i]) return 1;
  }

  return 0;
}

auto run_migration_if_needed(Core::State::AppState& app_state) -> bool {
  Logger().info("=== Migration Check Started ===");

  auto current_version = Vendor::Version::get_app_version();
  auto last_version_result = get_last_version();
  if (!last_version_result) {
    Logger().error("Failed to get last version: {}", last_version_result.error());
    return false;
  }

  std::string last_version = last_version_result.value();

  Logger().info("Version comparison: {} -> {}", last_version, current_version);

  // 如果版本相同，无需迁移
  if (compare_versions(last_version, current_version) == 0) {
    Logger().info("Already at version {}, no migration needed", current_version);
    return true;
  }

  // 收集需要执行的迁移脚本
  const auto& all_migrations = Scripts::get_all_migrations();
  std::vector<const Scripts::MigrationScript*> scripts_to_run;

  for (const auto& script : all_migrations) {
    // 选择版本号在 (last_version, current_version] 区间的脚本
    if (compare_versions(script.target_version, last_version) > 0 &&
        compare_versions(script.target_version, current_version) <= 0) {
      scripts_to_run.push_back(&script);
    }
  }

  // 按版本号排序（确保按顺序执行）
  std::ranges::sort(scripts_to_run, [](const auto* a, const auto* b) {
    return compare_versions(a->target_version, b->target_version) < 0;
  });

  if (scripts_to_run.empty()) {
    Logger().warn("No migration scripts found between {} and {}", last_version, current_version);
    Logger().warn("This might indicate a version downgrade or missing migration scripts");

    // 仍然保存当前版本号
    auto save_result = save_current_version(current_version);
    if (!save_result) {
      Logger().error("Failed to save version number: {}", save_result.error());
      return false;
    }

    return true;
  }

  // 执行迁移脚本
  Logger().info("Found {} migration script(s) to execute", scripts_to_run.size());

  for (const auto* script : scripts_to_run) {
    Logger().info("--- Executing migration to {} ---", script->target_version);
    Logger().info("Description: {}", script->description);

    auto result = script->migration_fn(app_state);

    if (!result) {
      Logger().error("Migration to {} failed: {}", script->target_version, result.error());
      Logger().error("=== Migration Failed ===");
      return false;
    }

    Logger().info("Migration to {} completed successfully", script->target_version);
  }

  // 保存新版本号
  auto save_result = save_current_version(current_version);
  if (!save_result) {
    Logger().error("Failed to save version number after successful migration: {}",
                   save_result.error());
    return false;
  }

  Logger().info("=== All Migrations Completed Successfully ===");
  return true;
}

}  // namespace Core::Migration
