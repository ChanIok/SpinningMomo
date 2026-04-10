module;

#include <asio.hpp>

module Features.Update;

import std;
import Core.Events;
import Core.Async;
import Core.Tasks;
import UI.FloatingWindow.Events;
import Core.State;
import Core.I18n.State;
import Core.HttpClient;
import Core.HttpClient.Types;
import Features.Update.State;
import Features.Update.Types;
import Features.Settings.State;
import Utils.Crypto;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.Throttle;
import Vendor.ShellApi;
import Vendor.Version;
import Vendor.Windows;

namespace Features::Update {

auto post_update_notification(Core::State::AppState& app_state, const std::string& message)
    -> void {
  if (!app_state.events || !app_state.i18n) {
    Logger().warn("Skip update notification: state is not ready");
    return;
  }

  auto app_name_it = app_state.i18n->texts.find("label.app_name");
  if (app_name_it == app_state.i18n->texts.end()) {
    Logger().warn("Skip update notification: app name text is missing");
    return;
  }

  Core::Events::post(*app_state.events, UI::FloatingWindow::Events::NotificationEvent{
                                            .title = app_name_it->second,
                                            .message = message,
                                        });
}

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

auto http_get(Core::State::AppState& app_state, const std::string& url)
    -> asio::awaitable<std::expected<std::string, std::string>> {
  Core::HttpClient::Types::Request request{
      .method = "GET",
      .url = url,
  };

  auto response_result = co_await Core::HttpClient::fetch(app_state, request);
  if (!response_result) {
    co_return std::unexpected("Failed to send HTTP request: " + response_result.error());
  }

  if (response_result->status_code != 200) {
    co_return std::unexpected("HTTP error: " + std::to_string(response_result->status_code));
  }

  co_return response_result->body;
}

auto get_temp_directory() -> std::expected<std::filesystem::path, std::string> {
  return Utils::Path::GetAppDataSubdirectory("temp");
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
  return Utils::Path::GetAppMode() == Utils::Path::AppMode::Portable;
}

constexpr auto kUpdateDownloadTaskType = "update.download";

auto make_task_progress(std::string stage, std::optional<std::string> message = std::nullopt,
                        std::optional<double> percent = std::nullopt) -> Core::Tasks::TaskProgress {
  return Core::Tasks::TaskProgress{
      .stage = std::move(stage),
      .current = 0,
      .total = 0,
      .percent = percent,
      .message = std::move(message),
  };
}

auto format_byte_size(std::uint64_t bytes) -> std::string {
  constexpr std::array<const char*, 5> kUnits = {"B", "KB", "MB", "GB", "TB"};
  auto value = static_cast<double>(bytes);
  std::size_t unit_index = 0;
  while (value >= 1024.0 && unit_index + 1 < kUnits.size()) {
    value /= 1024.0;
    ++unit_index;
  }

  if (unit_index == 0) {
    return std::format("{} {}", bytes, kUnits[unit_index]);
  }

  return std::format("{:.1f} {}", value, kUnits[unit_index]);
}

auto make_download_task_progress(const std::string& source_name,
                                 const Core::HttpClient::Types::DownloadProgress& progress)
    -> Core::Tasks::TaskProgress {
  std::optional<double> percent = std::nullopt;
  std::optional<std::string> message = std::nullopt;

  if (progress.total_bytes.has_value() && progress.total_bytes.value() > 0) {
    percent = std::clamp(static_cast<double>(progress.downloaded_bytes) * 100.0 /
                             static_cast<double>(progress.total_bytes.value()),
                         0.0, 100.0);
    message = std::format("{}: {} / {}", source_name, format_byte_size(progress.downloaded_bytes),
                          format_byte_size(progress.total_bytes.value()));
  } else {
    message = std::format("{}: {}", source_name, format_byte_size(progress.downloaded_bytes));
  }

  return Core::Tasks::TaskProgress{
      .stage = "download",
      .current = static_cast<std::int64_t>(progress.downloaded_bytes),
      .total = static_cast<std::int64_t>(progress.total_bytes.value_or(0)),
      .percent = percent,
      .message = std::move(message),
  };
}

// 从版本检查URL获取最新版本号
auto fetch_latest_version(Core::State::AppState& app_state, const std::string& version_url)
    -> asio::awaitable<std::expected<std::string, std::string>> {
  auto response = co_await http_get(app_state, version_url);
  if (!response) {
    co_return std::unexpected("Failed to fetch version info: " + response.error());
  }

  auto version = Utils::String::TrimAscii(response.value());
  if (version.empty()) {
    co_return std::unexpected("Empty version response");
  }

  co_return version;
}

auto download_file(Core::State::AppState& app_state, const std::string& url,
                   const std::filesystem::path& save_path,
                   Core::HttpClient::Types::DownloadProgressCallback progress_callback = nullptr)
    -> asio::awaitable<std::expected<void, std::string>> {
  try {
    Core::HttpClient::Types::Request request{
        .method = "GET",
        .url = url,
    };

    auto result = co_await Core::HttpClient::download_to_file(app_state, request, save_path,
                                                              progress_callback);
    if (!result) {
      co_return std::unexpected("Download failed: " + result.error());
    }

    co_return std::expected<void, std::string>{};

  } catch (const std::exception& e) {
    co_return std::unexpected("Download failed: " + std::string(e.what()));
  }
}

auto create_update_script(const std::filesystem::path& update_package_path, bool is_portable,
                          Vendor::Windows::DWORD target_pid, bool restart = true)
    -> std::expected<std::filesystem::path, std::string> {
  try {
    auto temp_dir = get_temp_directory();
    if (!temp_dir) {
      return std::unexpected("Failed to get temporary directory: " + temp_dir.error());
    }

    auto script_path = temp_dir.value() / std::filesystem::path("update.bat");

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
    script << "echo Waiting for application to exit...\n";
    script << "powershell -NoProfile -Command \"$pidToWait = " << target_pid
           << "; $timeoutMs = 15000; $process = Get-Process -Id $pidToWait -ErrorAction "
              "SilentlyContinue; if ($process) { if (-not $process.WaitForExit($timeoutMs)) "
              "{ Stop-Process -Id $pidToWait -Force -ErrorAction SilentlyContinue; "
              "Start-Sleep -Seconds 1 } }\"\n";

    if (is_portable) {
      // 便携版：解压zip覆盖当前目录
      script << "echo Extracting update package...\n";
      script << "powershell -Command \"Expand-Archive -Path '" << update_package_path.string()
             << "' -DestinationPath '" << current_dir.string() << "' -Force\"\n";
    } else {
      // 安装版：以最小界面执行安装，显示进度条并记录日志
      auto install_log_path =
          temp_dir.value() / std::filesystem::path("SpinningMomo-Update-Install.log");
      script << "echo Running installer...\n";
      script << "\"" << update_package_path.string() << "\" /passive /norestart /log \""
             << install_log_path.string() << "\"\n";
    }

    if (restart) {
      script << "echo Update completed, restarting application...\n";
      script << "start \"\" \""
             << (current_dir / std::filesystem::path("SpinningMomo.exe")).string() << "\"\n";
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

auto run_download_update_task(Core::State::AppState& app_state, const std::string& task_id,
                              const std::string& version, bool prepare_install_on_exit)
    -> asio::awaitable<void> {
  try {
    if (!app_state.update || !app_state.settings) {
      Core::Tasks::complete_task_failed(app_state, task_id, "Update state is not ready");
      co_return;
    }

    Core::Tasks::mark_task_running(app_state, task_id);
    // 下载开始时使旧的已下载版本标记失效，避免下载期间 install_update 误用旧文件
    app_state.update->downloaded_version.clear();

    const auto& download_sources = app_state.settings->raw.update.download_sources;
    if (download_sources.empty()) {
      Core::Tasks::complete_task_failed(app_state, task_id, "No download sources configured");
      co_return;
    }

    auto filename = get_update_filename(version, app_state.update->is_portable);
    auto temp_dir = get_temp_directory();
    if (!temp_dir) {
      auto error_message = "Failed to get temporary directory: " + temp_dir.error();
      Core::Tasks::complete_task_failed(app_state, task_id, error_message);
      co_return;
    }

    std::filesystem::path save_path = *temp_dir / filename;

    Core::Tasks::update_task_progress(
        app_state, task_id,
        make_task_progress("prepare", std::format("Preparing update package for {}", version),
                           0.0));

    // 按优先级依次尝试各下载源，任意一个成功即返回，全部失败才报错
    for (const auto& source : download_sources) {
      auto package_url_result = format_download_url(source.url_template, version, filename);
      if (!package_url_result) {
        Logger().warn("Skipped source {} due to invalid package URL template: {}", source.name,
                      package_url_result.error());
        continue;
      }

      auto checksums_url_result =
          format_download_url(source.url_template, version, "SHA256SUMS.txt");
      if (!checksums_url_result) {
        Logger().warn("Skipped source {} due to invalid checksum URL template: {}", source.name,
                      checksums_url_result.error());
        continue;
      }

      Core::Tasks::update_task_progress(
          app_state, task_id,
          make_task_progress("fetchChecksums",
                             std::format("Fetching checksums from {}", source.name), 5.0));

      auto checksums_content_result = co_await http_get(app_state, checksums_url_result.value());
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

      Core::Tasks::update_task_progress(
          app_state, task_id,
          make_task_progress("download", std::format("Trying download source: {}", source.name),
                             15.0));
      Logger().info("Trying download source: {} ({})", source.name, package_url_result.value());

      auto progress_throttle = Utils::Throttle::create<Core::HttpClient::Types::DownloadProgress>(
          std::chrono::milliseconds(250));
      auto emit_progress = [&app_state, &task_id,
                            &source](const Core::HttpClient::Types::DownloadProgress& progress) {
        Core::Tasks::update_task_progress(app_state, task_id,
                                          make_download_task_progress(source.name, progress));
      };

      auto download_result = co_await download_file(
          app_state, package_url_result.value(), save_path,
          [&progress_throttle,
           &emit_progress](const Core::HttpClient::Types::DownloadProgress& progress) {
            Utils::Throttle::call(*progress_throttle, emit_progress, progress);
          });
      Utils::Throttle::flush(*progress_throttle, emit_progress);
      if (!download_result) {
        Logger().warn("Download failed from {}: {}", source.name, download_result.error());
        continue;
      }

      Core::Tasks::update_task_progress(
          app_state, task_id,
          make_task_progress(
              "verify", std::format("Verifying downloaded package from {}", source.name), 85.0));

      auto verify_result = verify_downloaded_file_sha256(save_path, expected_sha256_result.value());
      if (!verify_result) {
        std::error_code remove_error;
        std::filesystem::remove(save_path, remove_error);
        Logger().warn("SHA256 verification failed from {}: {}", source.name, verify_result.error());
        continue;
      }

      // SHA256 校验通过后才标记下载完成，确保 install_update 只使用已验证的文件
      app_state.update->downloaded_version = version;

      if (prepare_install_on_exit) {
        Core::Tasks::update_task_progress(
            app_state, task_id,
            make_task_progress(
                "prepareInstall",
                std::format("Downloaded from {}. Preparing install on exit", source.name), 95.0));

        Types::InstallUpdateParams install_params;
        install_params.restart = false;
        auto install_result = install_update(app_state, install_params);
        if (!install_result) {
          auto error_message = "Failed to prepare downloaded update: " + install_result.error();
          Logger().warn("Startup auto update prepare failed: {}", install_result.error());
          Core::Tasks::complete_task_failed(app_state, task_id, error_message);
          co_return;
        }
      }

      Core::Tasks::update_task_progress(
          app_state, task_id,
          make_task_progress(
              "completed",
              prepare_install_on_exit
                  ? std::format("Downloaded from {} and scheduled for install on exit", source.name)
                  : std::format("Download completed from {}", source.name),
              100.0));

      Logger().info("Download completed from {}: {}", source.name, save_path.string());
      Core::Tasks::complete_task_success(app_state, task_id);
      co_return;
    }

    Core::Tasks::complete_task_failed(app_state, task_id, "All download sources failed");
  } catch (const std::exception& e) {
    Core::Tasks::complete_task_failed(app_state, task_id, std::string(e.what()));
  }
}

auto start_download_update_task(Core::State::AppState& app_state, bool prepare_install_on_exit)
    -> asio::awaitable<std::expected<Types::StartDownloadUpdateResult, std::string>> {
  if (!app_state.update) {
    co_return std::unexpected("Update not initialized");
  }

  if (app_state.update->latest_version.empty()) {
    co_return std::unexpected("No version info available. Please check for updates first.");
  }

  if (!app_state.settings) {
    co_return std::unexpected("Settings not initialized");
  }

  if (!app_state.async) {
    co_return std::unexpected("Async state is not initialized");
  }

  // 同一时刻只允许一个下载任务，重复调用直接返回已有任务 ID
  if (auto active_task = Core::Tasks::find_active_task_of_type(app_state, kUpdateDownloadTaskType);
      active_task.has_value()) {
    co_return Types::StartDownloadUpdateResult{
        .task_id = active_task->task_id,
        .status = "already_running",
    };
  }

  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    co_return std::unexpected("Async runtime is not available");
  }

  auto version = app_state.update->latest_version;
  auto task_id = Core::Tasks::create_task(app_state, kUpdateDownloadTaskType, version);
  if (task_id.empty()) {
    co_return std::unexpected("Failed to create update download task");
  }

  // co_await asio::post 将实际下载推迟到下一个事件循环周期，使本函数先返回给调用方
  asio::co_spawn(
      *io_context,
      [&app_state, task_id, version, prepare_install_on_exit]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);
        co_await run_download_update_task(app_state, task_id, version, prepare_install_on_exit);
      },
      asio::detached);

  co_return Types::StartDownloadUpdateResult{
      .task_id = task_id,
      .status = "started",
  };
}

auto schedule_startup_auto_update_check(Core::State::AppState& app_state) -> void {
  if (!app_state.settings || !app_state.update || !app_state.async) {
    Logger().warn("Skip startup auto update check: state is not ready");
    return;
  }

  if (!app_state.settings->raw.update.auto_check) {
    Logger().info("Skip startup auto update check: auto_check is disabled");
    return;
  }

  auto* io_context = Core::Async::get_io_context(*app_state.async);
  if (!io_context) {
    Logger().warn("Skip startup auto update check: async runtime is not ready");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);
        Logger().info("Startup auto update check started");

        auto check_result = co_await check_for_update(app_state);
        if (!check_result) {
          Logger().warn("Startup auto update check failed: {}", check_result.error());
          co_return;
        }

        if (check_result->has_update) {
          Logger().info("Startup auto update check found update: current={}, latest={}",
                        check_result->current_version, check_result->latest_version);

          if (!app_state.settings || !app_state.update) {
            Logger().warn("Skip startup auto update prepare: state is not ready");
            co_return;
          }

          if (!app_state.settings->raw.update.auto_update_on_exit) {
            Logger().info("Skip startup auto update prepare: auto_update_on_exit is disabled");
            if (app_state.i18n) {
              auto text_it = app_state.i18n->texts.find("message.update_available_about_prefix");
              if (text_it != app_state.i18n->texts.end()) {
                post_update_notification(app_state, text_it->second + check_result->latest_version);
              } else {
                Logger().warn("Skip update available notification: i18n text is missing");
              }
            }
            co_return;
          }

          if (app_state.update->pending_update) {
            Logger().info("Skip startup auto update prepare: pending update already exists");
            co_return;
          }

          auto download_task_result = co_await start_download_update_task(app_state, true);
          if (!download_task_result) {
            Logger().warn("Startup auto update download task failed: {}",
                          download_task_result.error());
            co_return;
          }

          Logger().info("Startup auto update background download {}: task_id={}",
                        download_task_result->status, download_task_result->task_id);
          co_return;
        }

        Logger().info("Startup auto update check completed: current version is up-to-date ({})",
                      check_result->current_version);
      },
      asio::detached);
}

auto check_for_update(Core::State::AppState& app_state)
    -> asio::awaitable<std::expected<Types::CheckUpdateResult, std::string>> {
  try {
    if (!app_state.update) {
      co_return std::unexpected("Update not initialized");
    }

    app_state.update->is_checking = true;
    app_state.update->error_message.clear();

    if (!app_state.settings) {
      app_state.update->is_checking = false;
      co_return std::unexpected("Settings not initialized");
    }

    // 从Cloudflare Pages获取最新版本号
    const auto& version_url = app_state.settings->raw.update.version_url;
    auto latest = co_await fetch_latest_version(app_state, version_url);
    if (!latest) {
      app_state.update->is_checking = false;
      app_state.update->error_message = latest.error();
      co_return std::unexpected(latest.error());
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
    // 已下载的版本与最新版本不符时清除，避免安装过期文件
    if (result.has_update && !app_state.update->downloaded_version.empty() &&
        app_state.update->downloaded_version != result.latest_version) {
      app_state.update->downloaded_version.clear();
    }

    Logger().info("Check for update: current={}, latest={}, has_update={}", current_version,
                  result.latest_version, result.has_update);

    co_return result;

  } catch (const std::exception& e) {
    if (app_state.update) {
      app_state.update->is_checking = false;
      app_state.update->error_message = e.what();
    }
    co_return std::unexpected(std::string(e.what()));
  }
}

auto execute_pending_update(Core::State::AppState& app_state) -> void {
  if (!app_state.update || !app_state.update->pending_update) {
    return;
  }

  const auto script_path = app_state.update->update_script_path;

  Logger().info("Executing pending update script: {}", script_path.string());

  const auto script_directory = script_path.parent_path();
  const auto command_parameters = std::format(L"/d /c \"{}\"", script_path.wstring());

  // 启动更新脚本
  Vendor::ShellApi::SHELLEXECUTEINFOW sei = {sizeof(sei)};
  sei.fMask = Vendor::ShellApi::kSEE_MASK_NOASYNC;
  sei.hwnd = nullptr;
  sei.lpVerb = L"open";
  sei.lpFile = L"cmd.exe";
  sei.lpParameters = command_parameters.c_str();
  sei.lpDirectory = script_directory.empty() ? nullptr : script_directory.c_str();
  sei.nShow = Vendor::ShellApi::kSW_HIDE;
  sei.hInstApp = nullptr;

  const bool shell_execute_ok = Vendor::ShellApi::ShellExecuteExW(&sei) != FALSE;

  if (shell_execute_ok) {
    Logger().info("Update script launch accepted by shell");
  } else {
    Logger().error("Failed to execute update script: last_error={}, script_path={}",
                   Vendor::Windows::GetLastError(), script_path.string());
  }

  // 清除待处理更新标志
  app_state.update->pending_update = false;
}

auto install_update(Core::State::AppState& app_state, const Types::InstallUpdateParams& params)
    -> std::expected<Types::InstallUpdateResult, std::string> {
  try {
    if (!app_state.update) {
      return std::unexpected("Update not initialized");
    }

    if (app_state.update->downloaded_version.empty()) {
      return std::unexpected("No downloaded update available");
    }

    if (!app_state.update->latest_version.empty() &&
        app_state.update->downloaded_version != app_state.update->latest_version &&
        app_state.update->update_available) {
      return std::unexpected(
          "Downloaded update is outdated. Please download the latest version again.");
    }

    // 根据安装类型确定更新包路径
    auto filename =
        get_update_filename(app_state.update->downloaded_version, app_state.update->is_portable);
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
        create_update_script(update_package_path, app_state.update->is_portable,
                             Vendor::Windows::GetCurrentProcessId(), params.restart);
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
