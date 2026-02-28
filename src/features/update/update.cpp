module;

module Features.Update;

import std;
import Core.Events;
import UI.FloatingWindow.Events;
import Core.State;
import Features.Update.State;
import Features.Update.Types;
import Features.Settings.State;
import Utils.Crypto;
import Utils.Logger;
import Utils.Path;
import Utils.String;
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

auto format_download_url(const std::string& url_template, const std::string& version,
                         const std::string& filename) -> std::expected<std::string, std::string> {
  try {
    return std::vformat(url_template, std::make_format_args(version, filename));
  } catch (const std::exception& e) {
    return std::unexpected("Invalid download URL template: " + std::string(e.what()));
  }
}

auto parse_sha256sum_for_filename(const std::string& checksums_content, const std::string& filename)
    -> std::expected<std::string, std::string> {
  std::istringstream stream(checksums_content);
  std::string line;
  while (std::getline(stream, line)) {
    auto trimmed_line = Utils::String::TrimAscii(line);
    if (trimmed_line.empty()) {
      continue;
    }

    size_t hash_end = 0;
    while (hash_end < trimmed_line.size() &&
           !std::isspace(static_cast<unsigned char>(trimmed_line[hash_end]))) {
      hash_end++;
    }
    if (hash_end == 0 || hash_end >= trimmed_line.size()) {
      continue;
    }

    auto hash = trimmed_line.substr(0, hash_end);
    if (hash.size() != 64 || !std::all_of(hash.begin(), hash.end(), [](unsigned char ch) {
          return std::isxdigit(ch) != 0;
        })) {
      continue;
    }

    while (hash_end < trimmed_line.size() &&
           std::isspace(static_cast<unsigned char>(trimmed_line[hash_end]))) {
      hash_end++;
    }

    if (hash_end < trimmed_line.size() && trimmed_line[hash_end] == '*') {
      hash_end++;
    }

    auto file_part = Utils::String::TrimAscii(trimmed_line.substr(hash_end));
    if (file_part == filename) {
      return Utils::String::ToLowerAscii(hash);
    }
  }

  return std::unexpected("SHA256SUMS does not contain checksum for " + filename);
}

auto verify_downloaded_file_sha256(const std::filesystem::path& file_path,
                                   const std::string& expected_sha256)
    -> std::expected<void, std::string> {
  auto actual_sha256 = Utils::Crypto::sha256_file(file_path);
  if (!actual_sha256) {
    return std::unexpected("Failed to calculate downloaded file hash: " + actual_sha256.error());
  }

  auto expected = Utils::String::ToLowerAscii(Utils::String::TrimAscii(expected_sha256));
  if (actual_sha256.value() != expected) {
    return std::unexpected("SHA256 mismatch");
  }

  return {};
}

// 根据安装类型获取更新文件名
auto get_update_filename(const std::string& version, bool is_portable) -> std::string {
  if (is_portable) {
    return "SpinningMomo-" + version + "-x64-Portable.zip";
  } else {
    return "SpinningMomo-" + version + "-x64-Setup.exe";
  }
}

// 检测是否为便携版安装（exe同目录下存在portable标记文件）
auto detect_portable() -> bool {
  auto exec_dir = Utils::Path::GetExecutableDirectory();
  if (!exec_dir) {
    return true;  // 默认为便携版
  }
  return std::filesystem::exists(*exec_dir / "portable");
}

// 从版本检查URL获取最新版本号
auto fetch_latest_version(const std::string& version_url)
    -> std::expected<std::string, std::string> {
  std::wstring wide_url(version_url.begin(), version_url.end());
  auto response = http_get(wide_url);
  if (!response) {
    return std::unexpected("Failed to fetch version info: " + response.error());
  }

  auto version = Utils::String::TrimAscii(response.value());
  if (version.empty()) {
    return std::unexpected("Empty version response");
  }

  return version;
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

auto create_update_script(const std::filesystem::path& update_package_path, bool is_portable,
                          bool restart = true)
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

    if (is_portable) {
      // 便携版：解压zip覆盖当前目录
      script << "echo Extracting update package...\n";
      script << "powershell -Command \"Expand-Archive -Path '" << update_package_path.string()
             << "' -DestinationPath '" << current_dir.string() << "' -Force\"\n";
    } else {
      // 安装版：运行安装程序静默升级
      script << "echo Running installer...\n";
      script << "\"" << update_package_path.string() << "\" /quiet\n";
    }

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
    if (!app_state.update) {
      return std::unexpected("Update state not created");
    }

    auto default_state = State::create_default_update_state();
    *app_state.update = std::move(default_state);

    // 检测安装类型
    app_state.update->is_portable = detect_portable();
    app_state.update->is_initialized = true;

    Logger().info("Update initialized successfully (portable: {})", app_state.update->is_portable);
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Failed to initialize update: " + std::string(e.what()));
  }
}

auto check_for_update(Core::State::AppState& app_state)
    -> std::expected<Types::CheckUpdateResult, std::string> {
  try {
    if (!app_state.update) {
      return std::unexpected("Update not initialized");
    }

    app_state.update->is_checking = true;
    app_state.update->error_message.clear();

    if (!app_state.settings) {
      app_state.update->is_checking = false;
      return std::unexpected("Settings not initialized");
    }

    // 从Cloudflare Pages获取最新版本号
    const auto& version_url = app_state.settings->raw.update.version_url;
    auto latest = fetch_latest_version(version_url);
    if (!latest) {
      app_state.update->is_checking = false;
      app_state.update->error_message = latest.error();
      return std::unexpected(latest.error());
    }

    auto current_version = Vendor::Version::get_app_version();

    Types::CheckUpdateResult result;
    result.latest_version = latest.value();
    result.current_version = current_version;
    result.has_update = is_update_needed(current_version, result.latest_version);

    // 更新状态
    app_state.update->is_checking = false;
    app_state.update->update_available = result.has_update;
    app_state.update->latest_version = result.latest_version;

    Logger().info("Check for update: current={}, latest={}, has_update={}", current_version,
                  result.latest_version, result.has_update);

    return result;

  } catch (const std::exception& e) {
    if (app_state.update) {
      app_state.update->is_checking = false;
      app_state.update->error_message = e.what();
    }
    return std::unexpected(std::string(e.what()));
  }
}

auto execute_pending_update(Core::State::AppState& app_state) -> void {
  if (!app_state.update || !app_state.update->pending_update) {
    return;
  }

  Logger().info("Executing pending update script: {}",
                app_state.update->update_script_path.string());

  // 启动更新脚本
  Vendor::ShellApi::SHELLEXECUTEINFOW sei = {sizeof(sei)};
  sei.fMask = Vendor::ShellApi::kSEE_MASK_NOCLOSEPROCESS;
  sei.hwnd = nullptr;
  sei.lpVerb = L"open";
  sei.lpFile = app_state.update->update_script_path.c_str();
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
  app_state.update->pending_update = false;
}

auto download_update(Core::State::AppState& app_state)
    -> std::expected<Types::DownloadUpdateResult, std::string> {
  try {
    if (!app_state.update) {
      return std::unexpected("Update not initialized");
    }

    if (app_state.update->latest_version.empty()) {
      return std::unexpected("No version info available. Please check for updates first.");
    }

    if (!app_state.settings) {
      return std::unexpected("Settings not initialized");
    }

    const auto& download_sources = app_state.settings->raw.update.download_sources;
    if (download_sources.empty()) {
      return std::unexpected("No download sources configured");
    }

    // 根据安装类型确定文件名和保存路径
    auto filename =
        get_update_filename(app_state.update->latest_version, app_state.update->is_portable);

    auto temp_dir = get_temp_directory();
    if (!temp_dir) {
      return std::unexpected("Failed to get temporary directory: " + temp_dir.error());
    }
    std::filesystem::path save_path = *temp_dir / filename;

    app_state.update->download_in_progress = true;
    app_state.update->download_progress = 0.0;
    app_state.update->error_message.clear();

    // 按优先级尝试各下载源
    for (size_t i = 0; i < download_sources.size(); ++i) {
      const auto& source = download_sources[i];
      auto package_url_result =
          format_download_url(source.url_template, app_state.update->latest_version, filename);
      if (!package_url_result) {
        Logger().warn("Skipped source {} due to invalid package URL template: {}", source.name,
                      package_url_result.error());
        continue;
      }

      auto checksums_url_result = format_download_url(
          source.url_template, app_state.update->latest_version, "SHA256SUMS.txt");
      if (!checksums_url_result) {
        Logger().warn("Skipped source {} due to invalid checksum URL template: {}", source.name,
                      checksums_url_result.error());
        continue;
      }

      std::wstring wide_checksums_url(checksums_url_result.value().begin(),
                                      checksums_url_result.value().end());
      auto checksums_content_result = http_get(wide_checksums_url);
      if (!checksums_content_result) {
        Logger().warn("Failed to fetch SHA256SUMS from {}: {}", source.name,
                      checksums_content_result.error());
        continue;
      }

      auto expected_sha256_result =
          parse_sha256sum_for_filename(checksums_content_result.value(), filename);
      if (!expected_sha256_result) {
        Logger().warn("Failed to parse SHA256SUMS from {}: {}", source.name,
                      expected_sha256_result.error());
        continue;
      }

      Logger().info("Trying download source: {} ({})", source.name, package_url_result.value());

      auto download_result = download_file(package_url_result.value(), save_path);
      if (download_result) {
        auto verify_result =
            verify_downloaded_file_sha256(save_path, expected_sha256_result.value());
        if (!verify_result) {
          std::error_code remove_error;
          std::filesystem::remove(save_path, remove_error);
          Logger().warn("SHA256 verification failed from {}: {}", source.name,
                        verify_result.error());
          continue;
        }

        app_state.update->download_in_progress = false;
        app_state.update->download_progress = 1.0;

        Types::DownloadUpdateResult result;
        result.message = "Download completed from " + source.name;
        result.file_path = save_path;

        Logger().info("Download completed from {}: {}", source.name, save_path.string());
        return result;
      }

      Logger().warn("Download failed from {}: {}", source.name, download_result.error());
    }

    // 所有源都失败
    app_state.update->download_in_progress = false;
    app_state.update->error_message = "All download sources failed";
    return std::unexpected("All download sources failed");

  } catch (const std::exception& e) {
    if (app_state.update) {
      app_state.update->download_in_progress = false;
      app_state.update->error_message = e.what();
    }
    return std::unexpected(std::string(e.what()));
  }
}

auto install_update(Core::State::AppState& app_state, const Types::InstallUpdateParams& params)
    -> std::expected<Types::InstallUpdateResult, std::string> {
  try {
    if (!app_state.update) {
      return std::unexpected("Update not initialized");
    }

    if (app_state.update->latest_version.empty()) {
      return std::unexpected("No version info available");
    }

    // 根据安装类型确定更新包路径
    auto filename =
        get_update_filename(app_state.update->latest_version, app_state.update->is_portable);
    auto temp_dir = get_temp_directory();
    if (!temp_dir) {
      return std::unexpected("Failed to get temporary directory: " + temp_dir.error());
    }
    std::filesystem::path update_package_path = *temp_dir / filename;

    if (!std::filesystem::exists(update_package_path)) {
      return std::unexpected("Update package does not exist: " + update_package_path.string());
    }

    Logger().info("Preparing update with package: {} (portable: {})", update_package_path.string(),
                  app_state.update->is_portable);

    auto script_result =
        create_update_script(update_package_path, app_state.update->is_portable, params.restart);
    if (!script_result) {
      return std::unexpected("Failed to create update script: " + script_result.error());
    }

    app_state.update->update_script_path = script_result.value();
    app_state.update->pending_update = true;

    Types::InstallUpdateResult result;

    if (params.restart) {
      Logger().info("Sending exit event for immediate update");
      Core::Events::post(*app_state.events, UI::FloatingWindow::Events::ExitEvent{});
      result.message = "Update will start immediately after application exits";
    } else {
      Logger().info("Update scheduled for program exit");
      result.message = "Update will be applied when the program exits";
    }

    return result;

  } catch (const std::exception& e) {
    return std::unexpected(std::string(e.what()));
  }
}

}  // namespace Features::Update
