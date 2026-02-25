module;

#include <wil/com.h>

module Plugins.InfinityNikki;

import std;
import Plugins.InfinityNikki.Types;
import Utils.Logger;
import Utils.String;
import Vendor.WIL;
import Vendor.Windows;
import Vendor.ShellApi;

namespace Plugins::InfinityNikki {

auto to_filesystem_path(const std::string& utf8_path) -> std::filesystem::path {
  auto wide_path = Utils::String::FromUtf8(utf8_path);
  if (!wide_path.empty()) {
    return std::filesystem::path(wide_path);
  }

  return std::filesystem::path(utf8_path);
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

// 获取用户配置文件路径
auto get_config_file_path() -> std::filesystem::path {
  wil::unique_cotaskmem_string local_app_data_path;

  HRESULT hr = Vendor::ShellApi::SHGetKnownFolderPath(
      Vendor::ShellApi::kFOLDERID_LocalAppData,  // 直接获取LocalAppData文件夹
      0,                                         // 默认标志
      nullptr,                                   // 当前用户
      &local_app_data_path                       // 输出路径
  );

  if (Vendor::Windows::_SUCCEEDED(hr) && local_app_data_path) {
    std::filesystem::path config_path = local_app_data_path.get();
    config_path /= L"InfinityNikki Launcher\\config.ini";
    return config_path;
  }

  // 如果获取失败，返回空路径
  return {};
}

// 使用 WIL 优化的配置文件读取函数
auto get_game_directory_from_config(const std::filesystem::path& config_path)
    -> std::expected<std::string, std::string> {
  try {
    // 固定大小的缓冲区对于路径来说通常足够
    constexpr Vendor::Windows::DWORD buffer_size =
        Vendor::Windows::kMAX_PATH * 2;  // 给予足够的空间
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

  } catch (const std::exception& ex) {
    return std::unexpected(std::string("Config reading failed: ") + ex.what());
  }
}

// 核心函数：获取游戏目录
auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string> {
  InfinityNikkiGameDirResult result;

  try {
    // 获取配置文件路径
    auto config_path = get_config_file_path();
    if (config_path.empty()) {
      result.config_found = false;
      result.game_dir_found = false;
      result.message = "Failed to get user profile path";
      return result;
    }

    Logger().info("Checking config file at: {}", config_path.string());

    // 检查配置文件是否存在
    if (!std::filesystem::exists(config_path)) {
      result.config_found = false;
      result.game_dir_found = false;
      result.message = "Config file not found at " + config_path.string();
      return result;
    }

    result.config_found = true;

    // 读取游戏目录配置
    auto game_dir_result = get_game_directory_from_config(config_path);
    if (!game_dir_result) {
      result.game_dir_found = false;
      result.message = game_dir_result.error();
      return result;
    }

    auto game_dir = game_dir_result.value();
    if (!has_valid_game_executable(game_dir)) {
      result.game_dir_found = false;
      result.message = "Invalid game directory: InfinityNikki.exe not found";
      Logger().warn("Detected gameDir is invalid: {}", game_dir);
      return result;
    }

    result.game_dir = game_dir;
    result.game_dir_found = true;
    result.message = "Game directory found successfully";

    Logger().info("Found Infinity Nikki game directory: {}", *result.game_dir);

    return result;

  } catch (const std::exception& ex) {
    return std::unexpected(std::string("Unexpected error: ") + ex.what());
  }
}

}  // namespace Plugins::InfinityNikki
