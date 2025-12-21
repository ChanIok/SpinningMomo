module;

#include <rfl/json.hpp>

module Features.Update;

import std;
import Core.Events;
import UI.FloatingWindow.Events;
import Core.State;
import Features.Update.State;
import Features.Update.Types;
import Features.Settings.State;
import Utils.Logger;
import Utils.Path;
import Vendor.ShellApi;
import Vendor.Version;
import Vendor.Windows;
import Vendor.WinHttp;

namespace Features::Update {

auto is_update_needed(const std::string& current_version, const std::string& latest_version)
    -> bool {
  // 简单的版本号比较，格式为 "x.y.z.w"
  auto split_version = [](const std::string& version) -> std::vector<int> {
    std::vector<int> parts;
    std::stringstream ss(version);
    std::string part;

    while (std::getline(ss, part, '.')) {
      try {
        parts.push_back(std::stoi(part));
      } catch (...) {
        parts.push_back(0);
      }
    }

    // 确保有4个部分
    while (parts.size() < 4) {
      parts.push_back(0);
    }

    return parts;
  };

  auto v1_parts = split_version(latest_version);
  auto v2_parts = split_version(current_version);

  for (size_t i = 0; i < 4; ++i) {
    if (v1_parts[i] > v2_parts[i]) {
      return true;
    } else if (v1_parts[i] < v2_parts[i]) {
      return false;
    }
  }

  return false;  // 版本相同
}

auto http_get(const std::wstring& url) -> std::expected<std::string, std::string> {
  Vendor::WinHttp::HINTERNET hSession = Vendor::WinHttp::WinHttpOpen(
      L"SpinningMomo/1.0", Vendor::WinHttp::kWINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      Vendor::WinHttp::kWINHTTP_NO_PROXY_NAME, Vendor::WinHttp::kWINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    return std::unexpected("Failed to open WinHTTP session");
  }

  Vendor::WinHttp::URL_COMPONENTS urlComponents = {sizeof(urlComponents)};
  wchar_t szHostName[256] = {0};
  wchar_t szUrlPath[1024] = {0};

  urlComponents.lpszHostName = szHostName;
  urlComponents.dwHostNameLength = sizeof(szHostName) / sizeof(wchar_t);
  urlComponents.lpszUrlPath = szUrlPath;
  urlComponents.dwUrlPathLength = sizeof(szUrlPath) / sizeof(wchar_t);

  if (!Vendor::WinHttp::WinHttpCrackUrl(url.c_str(), url.length(), 0, &urlComponents)) {
    Vendor::WinHttp::WinHttpCloseHandle(hSession);
    return std::unexpected("Failed to parse URL");
  }

  Vendor::WinHttp::HINTERNET hConnect =
      Vendor::WinHttp::WinHttpConnect(hSession, szHostName, urlComponents.nPort, 0);
  if (!hConnect) {
    Vendor::WinHttp::WinHttpCloseHandle(hSession);
    return std::unexpected("Failed to connect to server");
  }

  // 根据URL scheme判断是否需要HTTPS
  Vendor::WinHttp::DWORD flags = (urlComponents.nScheme == Vendor::WinHttp::kINTERNET_SCHEME_HTTPS)
                                     ? Vendor::WinHttp::kWINHTTP_FLAG_SECURE
                                     : 0;
  Vendor::WinHttp::HINTERNET hRequest = Vendor::WinHttp::WinHttpOpenRequest(
      hConnect, L"GET", szUrlPath, nullptr, Vendor::WinHttp::kWINHTTP_NO_REFERER,
      Vendor::WinHttp::kWINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest) {
    Vendor::WinHttp::WinHttpCloseHandle(hConnect);
    Vendor::WinHttp::WinHttpCloseHandle(hSession);
    return std::unexpected("Failed to open HTTP request");
  }

  if (!Vendor::WinHttp::WinHttpSendRequest(hRequest,
                                           Vendor::WinHttp::kWINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                           Vendor::WinHttp::kWINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    Vendor::WinHttp::WinHttpCloseHandle(hRequest);
    Vendor::WinHttp::WinHttpCloseHandle(hConnect);
    Vendor::WinHttp::WinHttpCloseHandle(hSession);
    return std::unexpected("Failed to send HTTP request");
  }

  if (!Vendor::WinHttp::WinHttpReceiveResponse(hRequest, nullptr)) {
    Vendor::WinHttp::WinHttpCloseHandle(hRequest);
    Vendor::WinHttp::WinHttpCloseHandle(hConnect);
    Vendor::WinHttp::WinHttpCloseHandle(hSession);
    return std::unexpected("Failed to receive HTTP response");
  }

  // 检查HTTP状态码
  Vendor::WinHttp::DWORD dwStatusCode = 0;
  Vendor::WinHttp::DWORD headerSize = sizeof(dwStatusCode);
  if (Vendor::WinHttp::WinHttpQueryHeaders(
          hRequest,
          Vendor::WinHttp::kWINHTTP_QUERY_STATUS_CODE | Vendor::WinHttp::kWINHTTP_QUERY_FLAG_NUMBER,
          Vendor::WinHttp::kWINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &headerSize, nullptr)) {
    if (dwStatusCode != 200) {
      Vendor::WinHttp::WinHttpCloseHandle(hRequest);
      Vendor::WinHttp::WinHttpCloseHandle(hConnect);
      Vendor::WinHttp::WinHttpCloseHandle(hSession);
      return std::unexpected("HTTP error: " + std::to_string(dwStatusCode));
    }
  }

  // 读取响应数据
  std::string response;
  Vendor::WinHttp::DWORD dwSize = 0;
  Vendor::WinHttp::DWORD dwDownloaded = 0;
  char buffer[4096];

  do {
    dwSize = 0;
    if (!Vendor::WinHttp::WinHttpQueryDataAvailable(hRequest, &dwSize)) {
      break;
    }

    if (dwSize == 0) {
      break;
    }

    std::memset(buffer, 0, sizeof(buffer));
    Vendor::WinHttp::DWORD bytesToRead =
        std::min(dwSize, static_cast<Vendor::WinHttp::DWORD>(sizeof(buffer)));
    if (!Vendor::WinHttp::WinHttpReadData(hRequest, buffer, bytesToRead, &dwDownloaded)) {
      break;
    }

    response.append(buffer, dwDownloaded);
  } while (dwSize > 0);

  Vendor::WinHttp::WinHttpCloseHandle(hRequest);
  Vendor::WinHttp::WinHttpCloseHandle(hConnect);
  Vendor::WinHttp::WinHttpCloseHandle(hSession);
  return response;
}

auto get_temp_directory() -> std::expected<std::filesystem::path, std::string> {
  auto exec_dir = Utils::Path::GetExecutableDirectory();
  if (!exec_dir) {
    return std::unexpected("Failed to get executable directory: " + exec_dir.error());
  }

  auto temp_dir = *exec_dir / "temp";
  auto create_result = Utils::Path::EnsureDirectoryExists(temp_dir);
  if (!create_result) {
    return std::unexpected("Failed to create temp directory: " + create_result.error());
  }

  return temp_dir;
}

auto parse_github_release_info(const std::string& json_response, const std::string& current_version)
    -> std::expected<Types::CheckUpdateResult, std::string> {
  try {
    auto release_result = rfl::json::read<Types::GitHubRelease>(json_response);
    if (!release_result) {
      return std::unexpected("Failed to parse GitHub release: " +
                             std::string(release_result.error().what()));
    }

    auto release = release_result.value();

    Types::CheckUpdateResult result;

    // 处理版本号
    result.latest_version = release.tag_name;
    if (!result.latest_version.empty() && result.latest_version[0] == 'v') {
      result.latest_version = result.latest_version.substr(1);
    }

    result.has_update = is_update_needed(current_version, result.latest_version);
    result.changelog = release.body;

    result.download_url.clear();
    for (const auto& asset : release.assets) {
      if (asset.name.starts_with("SpinningMomo-v") && asset.name.ends_with(".zip")) {
        result.download_url = asset.browser_download_url;
        break;
      }
    }

    // 如果没有找到匹配的文件，记录警告但仍然返回结果
    if (result.download_url.empty()) {
      Logger().warn("No matching SpinningMomo-v*.zip asset found in release");
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected("Failed to parse GitHub response: " + std::string(e.what()));
  }
}

auto get_release_info(Core::State::AppState& app_state)
    -> std::expected<Types::CheckUpdateResult, std::string> {
  try {
    // 从settings获取配置
    if (!app_state.settings || !app_state.updater) {
      return std::unexpected("Settings or updater not initialized");
    }

    const auto& updater_config = app_state.settings->raw.updater;
    if (updater_config.servers.empty()) {
      return std::unexpected("No update servers configured");
    }

    // 循环尝试所有服务器
    for (size_t i = 0; i < updater_config.servers.size(); ++i) {
      const auto& server = updater_config.servers[i];
      Logger().info("Attempting to use update server: {} (index: {})", server.name, i);

      // 使用配置的服务器URL
      std::wstring url(server.url.begin(), server.url.end());

      auto response = http_get(url);
      if (response) {
        // 成功获取响应，更新当前服务器索引并返回结果
        app_state.updater->current_server_index = i;
        Logger().info("Successfully connected to update server: {}", server.name);
        Logger().debug("Response: {}", response.value());
        return parse_github_release_info(response.value(), Vendor::Version::get_app_version());
      } else {
        // 记录失败，继续尝试下一个服务器
        Logger().warn("Failed to connect to update server {}: {}", server.name, response.error());
      }
    }

    // 所有服务器都尝试失败
    return std::unexpected("All update servers failed");

  } catch (const std::exception& e) {
    return std::unexpected("Failed to get release info: " + std::string(e.what()));
  }
}

auto download_file(const std::string& url, const std::filesystem::path& save_path)
    -> std::expected<void, std::string> {
  try {
    std::wstring wide_url(url.begin(), url.end());

    auto response = http_get(wide_url);
    if (!response) {
      return std::unexpected("Download failed: " + response.error());
    }

    std::ofstream file(save_path, std::ios::binary);
    if (!file) {
      return std::unexpected("Failed to create file: " + save_path.string());
    }

    file << *response;
    file.close();

    return {};

  } catch (const std::exception& e) {
    return std::unexpected("Download failed: " + std::string(e.what()));
  }
}

auto create_update_script(const std::filesystem::path& update_package_path, bool restart = true)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    auto temp_dir = get_temp_directory();
    if (!temp_dir) {
      return std::unexpected("Failed to get temporary directory: " + temp_dir.error());
    }

    auto script_path = Utils::Path::Combine(temp_dir.value(), "update.bat");

    std::ofstream script(script_path);
    if (!script) {
      return std::unexpected("Failed to create update script");
    }

    // 获取当前程序目录
    auto current_dir_result = Utils::Path::GetExecutableDirectory();
    if (!current_dir_result) {
      return std::unexpected("Failed to get executable directory: " + current_dir_result.error());
    }
    auto current_dir = current_dir_result.value();

    // 写入批处理脚本
    script << "@echo off\n";
    script << "echo Starting update process...\n";
    script << "timeout /t 1 /nobreak >nul\n";                  // 等待主程序退出
    script << "taskkill /f /im SpinningMomo.exe >nul 2>&1\n";  // 确保主程序退出
    script << "echo Extracting update package...\n";
    script << "powershell -Command \"Expand-Archive -Path '" << update_package_path.string()
           << "' -DestinationPath '" << current_dir.string() << "' -Force\"\n";

    if (restart) {
      script << "echo Update completed, restarting application...\n";
      script << "start \"\" \"" << Utils::Path::Combine(current_dir, "SpinningMomo.exe").string()
             << "\"\n";
    } else {
      script << "echo Update completed successfully.\n";
    }

    script << "echo Cleaning up temporary files...\n";
    script << "timeout /t 3 /nobreak >nul\n";
    script << "del \"" << update_package_path.string() << "\"\n";
    script << "del \"%~f0\"\n";  // 删除脚本自身

    script.close();

    return script_path;

  } catch (const std::exception& e) {
    return std::unexpected("Failed to create update script: " + std::string(e.what()));
  }
}

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  try {
    // 初始化已存在的更新状态
    if (!app_state.updater) {
      return std::unexpected("Updater state not created");
    }

    // 设置默认状态
    auto default_state = State::create_default_update_state();
    *app_state.updater = std::move(default_state);
    app_state.updater->is_initialized = true;

    Logger().info("Updater initialized successfully");
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to initialize updater: " + std::string(e.what()));
  }
}

auto check_for_update(Core::State::AppState& app_state)
    -> std::expected<Types::CheckUpdateResult, std::string> {
  try {
    if (!app_state.updater) {
      return std::unexpected("Updater not initialized");
    }

    // 更新状态为检查中
    app_state.updater->is_checking = true;
    app_state.updater->error_message.clear();

    // 调用内部函数获取发布信息（从settings获取配置）
    auto result = get_release_info(app_state);
    if (!result) {
      app_state.updater->is_checking = false;
      app_state.updater->error_message = result.error();
      return std::unexpected(result.error());
    }

    // 更新状态
    app_state.updater->is_checking = false;
    app_state.updater->update_available = result->has_update;
    app_state.updater->latest_version = result->latest_version;
    app_state.updater->changelog = result->changelog;
    app_state.updater->download_url = result->download_url;

    Logger().info("Check for update result: {}, {}, {}", result->latest_version, result->has_update,
                  result->download_url);

    return result;

  } catch (const std::exception& e) {
    if (app_state.updater) {
      app_state.updater->is_checking = false;
      app_state.updater->error_message = e.what();
    }
    return std::unexpected(std::string(e.what()));
  }
}

auto execute_pending_update(Core::State::AppState& app_state) -> void {
  if (!app_state.updater || !app_state.updater->pending_update) {
    return;
  }

  Logger().info("Executing pending update script: {}",
                app_state.updater->update_script_path.string());

  // 启动更新脚本
  Vendor::ShellApi::SHELLEXECUTEINFOW sei = {sizeof(sei)};
  sei.fMask = Vendor::ShellApi::kSEE_MASK_NOCLOSEPROCESS;
  sei.hwnd = nullptr;
  sei.lpVerb = L"open";
  sei.lpFile = app_state.updater->update_script_path.c_str();
  sei.lpParameters = nullptr;
  sei.lpDirectory = nullptr;
  sei.nShow = Vendor::ShellApi::kSW_HIDE;
  sei.hInstApp = nullptr;

  if (Vendor::ShellApi::ShellExecuteExW(&sei) && sei.hProcess) {
    // 关闭进程句柄
    Vendor::Windows::CloseHandle(sei.hProcess);
    Logger().info("Update script started successfully");
  } else {
    Logger().error("Failed to execute update script");
  }

  // 清除待处理更新标志
  app_state.updater->pending_update = false;
}

auto download_update(Core::State::AppState& app_state)
    -> std::expected<Types::DownloadUpdateResult, std::string> {
  try {
    if (!app_state.updater) {
      return std::unexpected("Updater not initialized");
    }

    // 检查是否有可用的下载URL
    if (app_state.updater->download_url.empty()) {
      return std::unexpected("No download URL available. Please check for updates first.");
    }

    // 确定保存路径 - 使用临时目录
    auto temp_dir = get_temp_directory();
    if (!temp_dir) {
      return std::unexpected("Failed to get temporary directory: " + temp_dir.error());
    }
    std::filesystem::path save_path = *temp_dir / "SpinningMomo_update.zip";

    // 更新状态
    app_state.updater->download_in_progress = true;
    app_state.updater->download_progress = 0.0;
    app_state.updater->error_message.clear();

    // 下载文件 - 使用状态中的download_url
    auto download_result = download_file(app_state.updater->download_url, save_path);
    if (!download_result) {
      app_state.updater->download_in_progress = false;
      app_state.updater->error_message = download_result.error();
      return std::unexpected(download_result.error());
    }

    // 更新状态
    app_state.updater->download_in_progress = false;
    app_state.updater->download_progress = 1.0;

    Types::DownloadUpdateResult result;
    result.message = "Download completed successfully";
    result.file_path = save_path;

    Logger().info("Download completed successfully: {}", save_path.string());
    return result;

  } catch (const std::exception& e) {
    if (app_state.updater) {
      app_state.updater->download_in_progress = false;
      app_state.updater->error_message = e.what();
    }
    return std::unexpected(std::string(e.what()));
  }
}

auto install_update(Core::State::AppState& app_state, const Types::InstallUpdateParams& params)
    -> std::expected<Types::InstallUpdateResult, std::string> {
  try {
    if (!app_state.updater) {
      return std::unexpected("Updater not initialized");
    }

    // 使用默认的更新包路径 - temp目录下的SpinningMomo_update.zip
    auto temp_dir = get_temp_directory();
    if (!temp_dir) {
      return std::unexpected("Failed to get temporary directory: " + temp_dir.error());
    }
    std::filesystem::path update_package_path = *temp_dir / "SpinningMomo_update.zip";

    // 验证更新包是否存在
    if (!std::filesystem::exists(update_package_path)) {
      return std::unexpected("Update package does not exist: " + update_package_path.string());
    }

    Logger().info("Preparing update process with package: {}", update_package_path.string());

    // 创建更新脚本，根据params.restart决定是否重启程序
    auto script_result = create_update_script(update_package_path, params.restart);
    if (!script_result) {
      return std::unexpected("Failed to create update script: " + script_result.error());
    }

    // 保存脚本路径到状态中
    app_state.updater->update_script_path = script_result.value();
    app_state.updater->pending_update = true;

    Types::InstallUpdateResult result;

    if (params.restart) {
      // 立即重启更新：发送退出事件
      Logger().info("Sending exit event for immediate update");
      Core::Events::post(*app_state.events, UI::FloatingWindow::Events::ExitEvent{});
      result.message = "Update will start immediately after application exits";
    } else {
      // 延迟更新：程序继续运行，更新会在退出时执行
      Logger().info("Update scheduled for program exit");
      result.message = "Update will be applied when the program exits";
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected(std::string(e.what()));
  }
}

}  // namespace Features::Update