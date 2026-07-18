module;

module Features.Backup;

import std;
import Core.Database;
import Core.State;
import Core.State.RuntimeInfo;
import Features.Backup.Types;
import Utils.Path;
import Utils.PowerShell;
import Utils.String;
import Vendor.Windows;

namespace Features::Backup::Detail {

// 返回当前 Unix 毫秒时间，用于生成不会互相覆盖的备份文件名。
auto now_millis() -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// 把 Windows 路径转换为 UTF-8，供 RPC 返回使用。
auto path_to_utf8(const std::filesystem::path& path) -> std::string {
  return Utils::String::ToUtf8(path.wstring());
}

// 写入备份中的版本文件和临时 PowerShell 脚本。
auto write_text_file(const std::filesystem::path& path, std::string_view content)
    -> std::expected<void, std::string> {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    return std::unexpected("Failed to open file for writing: " + path_to_utf8(path));
  }

  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.flush();
  if (!file) {
    return std::unexpected("Failed to write file: " + path_to_utf8(path));
  }
  return {};
}

// 创建位于 AppData/temp 下的本次备份工作目录。
auto create_operation_directory() -> std::expected<std::filesystem::path, std::string> {
  auto temp_directory_result = Utils::Path::GetAppDataSubdirectory("temp");
  if (!temp_directory_result) {
    return std::unexpected(temp_directory_result.error());
  }

  const auto operation_directory =
      *temp_directory_result /
      std::format("backup-{}-{}", now_millis(), Vendor::Windows::GetCurrentProcessId());
  std::error_code create_error;
  std::filesystem::create_directories(operation_directory, create_error);
  if (create_error) {
    return std::unexpected("Failed to create backup temp directory: " + create_error.message());
  }
  return operation_directory;
}

// 删除经过 AppData/temp 边界检查的工作目录。
auto remove_operation_directory(const std::filesystem::path& operation_directory) -> void {
  auto temp_directory_result = Utils::Path::GetAppDataSubdirectory("temp");
  if (!temp_directory_result ||
      !Utils::Path::IsPathWithinBase(operation_directory, *temp_directory_result)) {
    return;
  }

  std::error_code remove_error;
  std::filesystem::remove_all(operation_directory, remove_error);
}

// 创建把工作目录压缩为 ZIP 的 PowerShell 脚本。
auto write_compress_script(const std::filesystem::path& script_path)
    -> std::expected<void, std::string> {
  constexpr std::string_view script = R"PS(param(
  [Parameter(Mandatory=$true)][string]$SourceDirectory,
  [Parameter(Mandatory=$true)][string]$DestinationPath
)
$ErrorActionPreference = 'Stop'
$items = Get-ChildItem -LiteralPath $SourceDirectory -Force
Compress-Archive -Path $items.FullName -DestinationPath $DestinationPath -CompressionLevel Optimal -Force
)PS";
  return write_text_file(script_path, script);
}

// 创建只检查三个根文件是否存在的备份识别脚本。
auto write_validate_script(const std::filesystem::path& script_path)
    -> std::expected<void, std::string> {
  constexpr std::string_view script = R"PS(param(
  [Parameter(Mandatory=$true)][string]$ArchivePath
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
try {
  $entryNames = @($archive.Entries | ForEach-Object { $_.FullName -replace '\\', '/' })
  foreach ($requiredName in @('database.db', 'settings.json', 'app_version.txt')) {
    if ($entryNames -notcontains $requiredName) {
      throw "Required backup file is missing: $requiredName"
    }
  }
} finally {
  $archive.Dispose()
}
)PS";
  return write_text_file(script_path, script);
}

// 创建等待应用退出后删除旧数据、直接解压备份并重启应用的脚本。
auto write_restore_script(const std::filesystem::path& script_path)
    -> std::expected<void, std::string> {
  constexpr std::string_view script = R"PS(param(
  [Parameter(Mandatory=$true)][int]$PidToWait,
  [Parameter(Mandatory=$true)][string]$ArchivePath,
  [Parameter(Mandatory=$true)][string]$AppDataDirectory,
  [Parameter(Mandatory=$true)][string]$ExecutablePath
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
  throw "Backup archive not found: $ArchivePath"
}

$process = Get-Process -Id $PidToWait -ErrorAction SilentlyContinue
if ($process -and -not $process.WaitForExit(15000)) {
  Stop-Process -Id $PidToWait -Force
  Start-Sleep -Seconds 1
}

$targets = @(
  'database.db',
  'database.db-wal',
  'database.db-shm',
  'settings.json',
  'app_version.txt',
  'backgrounds'
)
foreach ($target in $targets) {
  $targetPath = Join-Path -Path $AppDataDirectory -ChildPath $target
  if (Test-Path -LiteralPath $targetPath) {
    Remove-Item -LiteralPath $targetPath -Recurse -Force
  }
}

Expand-Archive -LiteralPath $ArchivePath -DestinationPath $AppDataDirectory -Force
$workingDirectory = Split-Path -Parent $ExecutablePath
Start-Process -FilePath $ExecutablePath -WorkingDirectory $workingDirectory | Out-Null
Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue
)PS";
  return write_text_file(script_path, script);
}

// 复制当前设置文件到备份工作目录。
auto copy_settings(const std::filesystem::path& destination_path)
    -> std::expected<void, std::string> {
  auto settings_path_result = Utils::Path::GetAppDataFilePath("settings.json");
  if (!settings_path_result) {
    return std::unexpected(settings_path_result.error());
  }

  std::error_code copy_error;
  std::filesystem::copy_file(*settings_path_result, destination_path,
                             std::filesystem::copy_options::overwrite_existing, copy_error);
  if (copy_error) {
    return std::unexpected("Failed to copy settings.json: " + copy_error.message());
  }
  return {};
}

// 复制托管背景目录，恢复时会以该目录完整替换现有背景。
auto copy_backgrounds(const std::filesystem::path& destination_path)
    -> std::expected<void, std::string> {
  auto backgrounds_path_result = Utils::Path::GetAppDataSubdirectory("backgrounds");
  if (!backgrounds_path_result) {
    return std::unexpected(backgrounds_path_result.error());
  }

  std::error_code copy_error;
  std::filesystem::copy(
      *backgrounds_path_result, destination_path,
      std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
      copy_error);
  if (copy_error) {
    return std::unexpected("Failed to copy managed backgrounds: " + copy_error.message());
  }
  return {};
}

// 导出数据库快照、Settings、App Version 和托管背景到单个 ZIP。
auto export_backup_impl(Core::State::AppState& app_state, const Types::ExportParams& params)
    -> std::expected<Types::ExportResult, std::string> {
  const auto destination_directory =
      std::filesystem::path(Utils::String::FromUtf8(params.destination_directory));
  if (!std::filesystem::is_directory(destination_directory)) {
    return std::unexpected("Backup destination directory does not exist");
  }
  if (!app_state.runtime_info || app_state.runtime_info->version.empty()) {
    return std::unexpected("Current application version is unavailable");
  }

  auto operation_directory_result = create_operation_directory();
  if (!operation_directory_result) {
    return std::unexpected(operation_directory_result.error());
  }
  const auto operation_directory = *operation_directory_result;
  const auto payload_directory = operation_directory / L"payload";
  std::error_code create_error;
  std::filesystem::create_directories(payload_directory, create_error);
  if (create_error) {
    remove_operation_directory(operation_directory);
    return std::unexpected("Failed to create backup payload directory: " + create_error.message());
  }

  // SQLiteCpp 在线备份会把 WAL 中的当前状态合并为独立数据库快照。
  auto database_result = Core::Database::backup_to(app_state, payload_directory / L"database.db");
  if (!database_result) {
    remove_operation_directory(operation_directory);
    return std::unexpected(database_result.error());
  }

  auto settings_result = copy_settings(payload_directory / L"settings.json");
  if (!settings_result) {
    remove_operation_directory(operation_directory);
    return std::unexpected(settings_result.error());
  }
  auto backgrounds_result = copy_backgrounds(payload_directory / L"backgrounds");
  if (!backgrounds_result) {
    remove_operation_directory(operation_directory);
    return std::unexpected(backgrounds_result.error());
  }
  auto version_result =
      write_text_file(payload_directory / L"app_version.txt", app_state.runtime_info->version);
  if (!version_result) {
    remove_operation_directory(operation_directory);
    return std::unexpected(version_result.error());
  }

  const auto created_at = now_millis();
  std::wstring safe_version = Utils::String::FromUtf8(app_state.runtime_info->version);
  std::ranges::replace_if(
      safe_version,
      [](wchar_t character) {
        return !std::iswalnum(character) && character != L'.' && character != L'-';
      },
      L'_');
  const auto final_path = destination_directory /
                          std::format(L"SpinningMomo-Backup-v{}-{}.zip", safe_version, created_at);
  const auto temporary_path =
      destination_directory / std::format(L"SpinningMomo-Backup-{}.partial.zip", created_at);
  const auto script_path = operation_directory / L"compress-backup.ps1";

  auto script_result = write_compress_script(script_path);
  if (!script_result) {
    remove_operation_directory(operation_directory);
    return std::unexpected(script_result.error());
  }
  auto compress_result = Utils::PowerShell::run_script_and_wait(
      script_path, {L"-SourceDirectory", payload_directory.wstring(), L"-DestinationPath",
                    temporary_path.wstring()});
  if (!compress_result || *compress_result != 0) {
    std::error_code remove_error;
    std::filesystem::remove(temporary_path, remove_error);
    remove_operation_directory(operation_directory);
    return std::unexpected(compress_result ? "Windows PowerShell failed to create backup"
                                           : compress_result.error());
  }

  std::error_code rename_error;
  std::filesystem::rename(temporary_path, final_path, rename_error);
  if (rename_error) {
    std::error_code remove_error;
    std::filesystem::remove(temporary_path, remove_error);
    remove_operation_directory(operation_directory);
    return std::unexpected("Failed to publish backup archive: " + rename_error.message());
  }

  std::error_code size_error;
  const auto archive_size = std::filesystem::file_size(final_path, size_error);
  remove_operation_directory(operation_directory);
  if (size_error) {
    return std::unexpected("Failed to read backup archive size: " + size_error.message());
  }
  return Types::ExportResult{
      .backup_path = path_to_utf8(final_path),
      .app_version = app_state.runtime_info->version,
      .created_at = created_at,
      .size = archive_size,
  };
}

// 准备恢复脚本，脚本会在当前进程退出后直接替换 AppData 中的数据文件。
auto restore_backup_impl(const Types::RestoreParams& params)
    -> std::expected<Types::RestoreResult, std::string> {
  const auto archive_path = std::filesystem::path(Utils::String::FromUtf8(params.backup_path));
  if (!std::filesystem::is_regular_file(archive_path)) {
    return std::unexpected("Backup archive does not exist");
  }

  auto extension = archive_path.extension().wstring();
  std::ranges::transform(extension, extension.begin(),
                         [](wchar_t character) { return std::towlower(character); });
  if (extension != L".zip") {
    return std::unexpected("Backup archive must be a .zip file");
  }

  auto temp_directory_result = Utils::Path::GetAppDataSubdirectory("temp");
  auto app_data_result = Utils::Path::GetAppDataDirectory();
  auto executable_path_result = Utils::Path::GetExecutablePath();
  if (!temp_directory_result || !app_data_result || !executable_path_result) {
    return std::unexpected("Failed to prepare application restore paths");
  }

  // 退出应用前只识别三个必需根文件，防止用户误选普通 ZIP 后删除当前数据。
  const auto validate_script_path =
      *temp_directory_result / std::format("validate-backup-{}.ps1", now_millis());
  auto validate_script_result = write_validate_script(validate_script_path);
  if (!validate_script_result) {
    return std::unexpected(validate_script_result.error());
  }
  auto validate_result = Utils::PowerShell::run_script_and_wait(
      validate_script_path, {L"-ArchivePath", archive_path.wstring()});
  std::error_code remove_error;
  std::filesystem::remove(validate_script_path, remove_error);
  if (!validate_result) {
    return std::unexpected(validate_result.error());
  }
  if (*validate_result != 0) {
    return std::unexpected(
        "Selected ZIP is not a SpinningMomo backup: database.db, settings.json or "
        "app_version.txt is missing");
  }

  const auto script_path =
      *temp_directory_result / std::format("restore-backup-{}.ps1", now_millis());
  auto script_result = write_restore_script(script_path);
  if (!script_result) {
    return std::unexpected(script_result.error());
  }

  auto launch_result = Utils::PowerShell::launch_script(
      script_path,
      {L"-PidToWait", std::to_wstring(Vendor::Windows::GetCurrentProcessId()), L"-ArchivePath",
       archive_path.wstring(), L"-AppDataDirectory", app_data_result->wstring(), L"-ExecutablePath",
       executable_path_result->wstring()});
  if (!launch_result) {
    std::error_code remove_error;
    std::filesystem::remove(script_path, remove_error);
    return std::unexpected(launch_result.error());
  }

  return Types::RestoreResult{.scheduled = true};
}

}  // namespace Features::Backup::Detail

namespace Features::Backup {

// 导出数据库快照、Settings、App Version 和托管背景到单个 ZIP。
auto export_backup(Core::State::AppState& app_state, const Types::ExportParams& params)
    -> std::expected<Types::ExportResult, std::string> {
  try {
    return Detail::export_backup_impl(app_state, params);
  } catch (const std::exception& e) {
    return std::unexpected("Backup export failed: " + std::string(e.what()));
  }
}

// 启动完全替换恢复脚本，当前进程退出后直接解压备份并重启应用。
auto restore_backup(const Types::RestoreParams& params)
    -> std::expected<Types::RestoreResult, std::string> {
  try {
    return Detail::restore_backup_impl(params);
  } catch (const std::exception& e) {
    return std::unexpected("Backup restore failed: " + std::string(e.what()));
  }
}

}  // namespace Features::Backup
