module;

#include <wil/com.h>

module Extensions.InfinityNikki.GameDirectory;

import std;
import Extensions.InfinityNikki.Types;
import Utils.Logger;
import Utils.String;
import Vendor.ShellApi;
import Vendor.Windows;

namespace Extensions::InfinityNikki::GameDirectory {

auto to_filesystem_path(const std::string& utf8_path) -> std::filesystem::path {
  return std::filesystem::path(Utils::String::FromUtf8(utf8_path));
}

auto has_valid_game_executable(const std::string& game_dir_utf8) -> bool {
  auto game_dir_path = to_filesystem_path(game_dir_utf8);
  if (game_dir_path.empty() || !std::filesystem::exists(game_dir_path) ||
      !std::filesystem::is_directory(game_dir_path)) {
    return false;
  }

  auto game_exe_path = game_dir_path / L"InfinityNikki.exe";
  return std::filesystem::exists(game_exe_path) && std::filesystem::is_regular_file(game_exe_path);
}

auto get_game_directory_from_config(const std::filesystem::path& config_path)
    -> std::expected<std::string, std::string> {
  constexpr Vendor::Windows::DWORD buffer_size = Vendor::Windows::kMAX_PATH * 2;
  auto buffer = wil::make_unique_hlocal_nothrow<wchar_t[]>(buffer_size);
  if (!buffer) {
    return std::unexpected("Memory allocation failed");
  }

  Vendor::Windows::DWORD result = Vendor::Windows::GetPrivateProfileStringW(
      L"Download", L"gameDir", L"", buffer.get(), buffer_size, config_path.wstring().c_str());
  if (result == 0) {
    return std::unexpected("gameDir not found in config file");
  }

  return Utils::String::ToUtf8(buffer.get());
}

auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string> {
  InfinityNikkiGameDirResult result;

  wil::unique_cotaskmem_string local_app_data_path;
  HRESULT hr = Vendor::ShellApi::SHGetKnownFolderPath(Vendor::ShellApi::kFOLDERID_LocalAppData, 0,
                                                      nullptr, &local_app_data_path);
  if (!Vendor::Windows::_SUCCEEDED(hr) || !local_app_data_path) {
    result.message = "Failed to get user profile path";
    return result;
  }

  const std::filesystem::path local_app_data_dir = local_app_data_path.get();
  constexpr std::array launcher_names = {
      L"InfinityNikki Launcher",
      L"InfinityNikkiGlobal Launcher",
      L"InfinityNikkiBili Launcher",
      L"InfinityNikkiSteam Launcher",
  };

  std::string last_error = "No valid launcher config found";
  for (const auto* launcher_name : launcher_names) {
    auto config_path = local_app_data_dir / launcher_name / L"config.ini";

    Logger().info("Checking config file at: {}", config_path.string());

    if (!std::filesystem::exists(config_path)) {
      last_error = "Config file not found at " + config_path.string();
      continue;
    }

    result.config_found = true;

    auto game_dir_result = get_game_directory_from_config(config_path);
    if (!game_dir_result) {
      last_error = game_dir_result.error();
      Logger().warn("Failed to read gameDir from config: {}", config_path.string());
      continue;
    }

    auto game_dir = game_dir_result.value();
    if (!has_valid_game_executable(game_dir)) {
      last_error = "Invalid game directory: InfinityNikki.exe not found";
      Logger().warn("Detected gameDir is invalid: {}", game_dir);
      continue;
    }

    result.game_dir = game_dir;
    result.game_dir_found = true;
    result.message = "Game directory found successfully";
    Logger().info("Found Infinity Nikki game directory: {}", *result.game_dir);
    return result;
  }

  result.message = last_error;
  return result;
}

}  // namespace Extensions::InfinityNikki::GameDirectory
