#include "features/gallery/download/download.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"

#include "features/gallery/asset/repository.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"
#include "utils/powershell/powershell.hpp"
#include "utils/string/string.hpp"

namespace features::gallery::download {
namespace {

constexpr std::string_view kDownloadDirectoryName = "gallery-downloads";
constexpr std::string_view kArchiveFileName = "SpinningMomo-Download.zip";
std::atomic<std::uint64_t> archive_sequence = 0;

struct PreparedAsset {
  std::int64_t id = 0;
  DownloadFile file;
};

// 将 Windows 路径转换成日志和脚本使用的 UTF-8 文本。
auto path_to_utf8(const std::filesystem::path& path) -> std::string {
  return utils::string::ToUtf8(path.wstring());
}

// 确认路径当前指向可读取的普通文件。
auto is_regular_file_available(const std::filesystem::path& path) -> bool {
  std::error_code error;
  const auto regular = std::filesystem::is_regular_file(path, error);
  return regular && !error;
}

// 生成不会与其他下载操作冲突的归档令牌。
auto make_download_token() -> std::string {
  std::random_device random_device;
  const auto sequence = archive_sequence.fetch_add(1, std::memory_order_relaxed);
  const auto ticks =
      static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

  return std::format("{:016x}-{:016x}-{:08x}", ticks, sequence,
                     static_cast<std::uint32_t>(random_device()));
}

// 限制归档文件名，确保 URL 不能逃出下载目录。
auto is_safe_archive_name(std::string_view archive_name) -> bool {
  if (archive_name.size() <= 4 || !archive_name.ends_with(".zip")) {
    return false;
  }

  return std::ranges::all_of(archive_name, [](unsigned char character) {
    return std::isalnum(character) || character == '-' || character == '_' || character == '.';
  });
}

// 从数据库解析资产路径，并在返回前确认文件仍存在。
auto resolve_asset_file_impl(core::AppState& app_state, std::int64_t asset_id)
    -> std::expected<DownloadFile, std::string> {
  if (asset_id <= 0) {
    return std::unexpected("Invalid asset id");
  }

  auto asset_result = asset::repository::get_asset_by_id(app_state, asset_id);
  if (!asset_result) {
    return std::unexpected(asset_result.error());
  }
  if (!asset_result->has_value()) {
    return std::unexpected("Asset was not found");
  }

  const auto source_path = std::filesystem::path(utils::string::FromUtf8((*asset_result)->path));
  if (!is_regular_file_available(source_path)) {
    return std::unexpected("Asset file is not available");
  }

  const auto file_name = source_path.filename();
  if (file_name.empty() || file_name == L"." || file_name == L"..") {
    return std::unexpected("Asset file name is invalid");
  }

  return DownloadFile{
      .file_path = source_path,
      .file_name = utils::string::ToUtf8(file_name.wstring()),
  };
}

// 生成大小写不敏感的比较键，用于归档内文件名去重。
auto lower_file_name(std::wstring value) -> std::wstring {
  std::ranges::transform(value, value.begin(),
                         [](wchar_t character) { return std::towlower(character); });
  return value;
}

// 为 ZIP 内的文件名追加后缀，避免同名条目互相覆盖。
auto make_unique_archive_name(const std::wstring& requested_name,
                              std::unordered_set<std::wstring>& used_names)
    -> std::expected<std::wstring, std::string> {
  const auto requested_path = std::filesystem::path(requested_name).filename();
  if (requested_path.empty() || requested_path == L"." || requested_path == L"..") {
    return std::unexpected("Archive file name is invalid");
  }

  for (std::uint64_t suffix = 0; suffix < 10'000; ++suffix) {
    auto candidate = requested_path;
    if (suffix > 0) {
      candidate = std::filesystem::path(std::format(L"{} ({}){}", requested_path.stem().wstring(),
                                                    suffix, requested_path.extension().wstring()));
    }

    if (used_names.insert(lower_file_name(candidate.wstring())).second) {
      return candidate.wstring();
    }
  }

  return std::unexpected("Too many archive file name collisions");
}

// 将归档准备所需的清单或脚本完整写入临时文件。
auto write_text_file(const std::filesystem::path& path, std::string_view content)
    -> std::expected<void, std::string> {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    return std::unexpected("Failed to open download preparation file: " + path_to_utf8(path));
  }

  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.flush();
  if (!file) {
    return std::unexpected("Failed to write download preparation file: " + path_to_utf8(path));
  }
  return {};
}

// 写入使用无压缩模式复制媒体文件的 PowerShell 归档脚本。
auto write_archive_script(const std::filesystem::path& script_path)
    -> std::expected<void, std::string> {
  constexpr std::string_view script = R"PS(param(
  [Parameter(Mandatory=$true)][string]$ManifestPath,
  [Parameter(Mandatory=$true)][string]$DestinationPath
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (Test-Path -LiteralPath $DestinationPath -PathType Leaf) {
  Remove-Item -LiteralPath $DestinationPath -Force
}

$archive = [IO.Compression.ZipFile]::Open(
  $DestinationPath,
  [IO.Compression.ZipArchiveMode]::Create
)
try {
  foreach ($line in Get-Content -LiteralPath $ManifestPath -Encoding UTF8) {
    if ([string]::IsNullOrWhiteSpace($line)) {
      continue
    }

    $separator = $line.IndexOf([char]9)
    if ($separator -lt 1 -or $separator -ge ($line.Length - 1)) {
      throw "Invalid gallery download manifest entry"
    }

    $sourcePath = $line.Substring(0, $separator)
    $entryName = $line.Substring($separator + 1)
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
      throw "Gallery asset disappeared during archive preparation: $sourcePath"
    }

    $entry = $archive.CreateEntry(
      $entryName,
      [IO.Compression.CompressionLevel]::NoCompression
    )
    $sourceStream = $null
    $entryStream = $null
    try {
      $sourceStream = [IO.File]::OpenRead($sourcePath)
      $entryStream = $entry.Open()
      $sourceStream.CopyTo($entryStream)
    } finally {
      if ($entryStream) { $entryStream.Dispose() }
      if ($sourceStream) { $sourceStream.Dispose() }
    }
  }
} finally {
  $archive.Dispose()
}
)PS";

  return write_text_file(script_path, script);
}

// 为一次归档准备创建隔离的临时操作目录。
auto create_operation_directory(const std::string& token)
    -> std::expected<std::filesystem::path, std::string> {
  auto temp_directory_result = utils::path::GetAppDataSubdirectory("temp");
  if (!temp_directory_result) {
    return std::unexpected(temp_directory_result.error());
  }

  const auto operation_directory =
      *temp_directory_result / (L"gallery-download-" + utils::string::FromUtf8(token));
  std::error_code create_error;
  std::filesystem::create_directories(operation_directory, create_error);
  if (create_error) {
    return std::unexpected("Failed to create download preparation directory: " +
                           create_error.message());
  }
  return operation_directory;
}

// 删除一次归档准备使用的临时目录，并限制清理范围在 temp 根目录内。
auto remove_operation_directory(const std::filesystem::path& operation_directory) -> void {
  try {
    auto temp_directory_result = utils::path::GetAppDataSubdirectory("temp");
    if (!temp_directory_result ||
        !utils::path::IsPathWithinBase(operation_directory, *temp_directory_result)) {
      return;
    }

    std::error_code remove_error;
    std::filesystem::remove_all(operation_directory, remove_error);
    if (remove_error) {
      Logger().warn("Failed to clean download preparation directory '{}': {}",
                    path_to_utf8(operation_directory), remove_error.message());
    }
  } catch (const std::exception& error) {
    Logger().warn("Failed to clean download preparation directory '{}': {}",
                  path_to_utf8(operation_directory), error.what());
  }
}

// 按调用方给出的规则删除目录中的遗留下载项。
auto remove_matching_entries(const std::filesystem::path& directory, auto&& should_remove) -> void {
  std::error_code iterator_error;
  for (const auto& entry : std::filesystem::directory_iterator(directory, iterator_error)) {
    if (iterator_error) {
      break;
    }
    if (!should_remove(entry.path())) {
      continue;
    }

    std::error_code remove_error;
    std::filesystem::remove_all(entry.path(), remove_error);
    if (remove_error) {
      Logger().warn("Failed to remove stale gallery download entry '{}': {}",
                    path_to_utf8(entry.path()), remove_error.message());
    }
  }

  if (iterator_error) {
    Logger().warn("Failed to enumerate stale gallery download entries '{}': {}",
                  path_to_utf8(directory), iterator_error.message());
  }
}

// 写入媒体清单、创建 ZIP，并以原子改名发布可下载归档。
auto create_archive(const std::vector<PreparedAsset>& assets)
    -> std::expected<std::string, std::string> {
  // 归档文件和准备脚本分别放入持久下载目录与隔离临时目录。
  auto download_directory_result = utils::path::GetAppDataSubdirectory(kDownloadDirectoryName);
  if (!download_directory_result) {
    return std::unexpected(download_directory_result.error());
  }
  const auto& download_directory = *download_directory_result;

  const auto token = make_download_token();
  auto operation_directory_result = create_operation_directory(token);
  if (!operation_directory_result) {
    return std::unexpected(operation_directory_result.error());
  }
  const auto operation_directory = *operation_directory_result;
  const auto manifest_path = operation_directory / L"manifest.txt";
  const auto script_path = operation_directory / L"create-archive.ps1";
  const auto token_path = std::filesystem::path(utils::string::FromUtf8(token));
  const auto temporary_archive_path = download_directory / (token_path.wstring() + L".partial.zip");
  const auto final_archive_path = download_directory / (token_path.wstring() + L".zip");
  // 无论准备在哪一步失败，都回收半成品和本次操作目录。
  [[maybe_unused]] auto cleanup_guard = wil::scope_exit([&] {
    std::error_code remove_error;
    std::filesystem::remove(temporary_archive_path, remove_error);
    if (remove_error) {
      Logger().debug("Failed to remove partial gallery archive '{}': {}",
                     path_to_utf8(temporary_archive_path), remove_error.message());
    }
    remove_operation_directory(operation_directory);
  });

  std::string manifest_content = "\xEF\xBB\xBF";
  // 用绝对源路径和 ZIP 内文件名组成 PowerShell 清单。
  for (const auto& asset : assets) {
    manifest_content += path_to_utf8(asset.file.file_path);
    manifest_content.push_back('\t');
    manifest_content += asset.file.file_name;
    manifest_content.push_back('\n');
  }

  // 先落盘清单，再生成归档脚本。
  auto manifest_result = write_text_file(manifest_path, manifest_content);
  if (!manifest_result) {
    return std::unexpected(manifest_result.error());
  }

  auto script_result = write_archive_script(script_path);
  if (!script_result) {
    return std::unexpected(script_result.error());
  }

  // 在 WorkerPool 中同步等待 PowerShell 完成，避免 RPC 线程被磁盘操作阻塞。
  auto compress_result = utils::powershell::run_script_and_wait(
      script_path, {L"-ManifestPath", manifest_path.wstring(), L"-DestinationPath",
                    temporary_archive_path.wstring()});
  if (!compress_result || *compress_result != 0) {
    return std::unexpected(compress_result ? "Windows PowerShell failed to create gallery archive"
                                           : compress_result.error());
  }

  // 只有完整归档生成后才暴露最终 .zip 文件名。
  std::error_code rename_error;
  std::filesystem::rename(temporary_archive_path, final_archive_path, rename_error);
  if (rename_error) {
    return std::unexpected("Failed to publish gallery archive: " + rename_error.message());
  }

  return token;
}

}  // namespace

// 清理上一次进程遗留的归档文件、半成品和准备目录。
auto cleanup_stale_files() -> void {
  try {
    // 下载目录中的全部文件都是一次性归档，启动时可以整体回收。
    auto download_directory_result = utils::path::GetAppDataSubdirectory(kDownloadDirectoryName);
    if (!download_directory_result) {
      Logger().warn("Failed to locate gallery download directory for startup cleanup: {}",
                    download_directory_result.error());
    } else {
      remove_matching_entries(*download_directory_result, [](const auto&) { return true; });
    }

    // 只清理由 Gallery 创建的准备目录，不触碰 temp 下的其他功能数据。
    auto temp_directory_result = utils::path::GetAppDataSubdirectory("temp");
    if (!temp_directory_result) {
      Logger().warn(
          "Failed to locate gallery download preparation directory for startup cleanup: {}",
          temp_directory_result.error());
      return;
    }

    remove_matching_entries(*temp_directory_result, [](const auto& path) {
      return path.filename().wstring().starts_with(L"gallery-download-");
    });
  } catch (const std::exception& error) {
    Logger().warn("Gallery download startup cleanup failed: {}", error.what());
  } catch (...) {
    Logger().warn("Gallery download startup cleanup failed with an unknown error");
  }
}

// 去重并校验资产，单文件直链或多文件 ZIP 归档由结果数量决定。
auto prepare(core::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<PrepareDownloadResult, std::string> {
  try {
    PrepareDownloadResult result;
    std::unordered_set<std::int64_t> seen_ids;
    std::vector<PreparedAsset> available_assets;
    seen_ids.reserve(ids.size());
    available_assets.reserve(ids.size());

    // 逐个重新解析磁盘文件，允许失效资产被跳过而不影响其他文件。
    for (const auto asset_id : ids) {
      if (asset_id <= 0 || !seen_ids.insert(asset_id).second) {
        continue;
      }

      auto file_result = resolve_asset_file_impl(app_state, asset_id);
      if (!file_result) {
        result.failed_count++;
        Logger().debug("Skipping unavailable gallery asset {} for download: {}", asset_id,
                       file_result.error());
        continue;
      }
      available_assets.push_back(
          PreparedAsset{.id = asset_id, .file = std::move(file_result.value())});
    }

    // 没有任何可用文件时返回失败计数，不创建空归档。
    if (available_assets.empty()) {
      return result;
    }

    // 只有单个请求且没有失败项时才直接返回原始文件。
    const auto unique_requested_count = seen_ids.size();
    if (unique_requested_count == 1 && result.failed_count == 0) {
      result.asset_id = available_assets.front().id;
      result.file_name = available_assets.front().file.file_name;
      return result;
    }

    // 先固定 ZIP 内名称，保证重复文件名不会覆盖。
    std::unordered_set<std::wstring> used_names;
    for (auto& asset : available_assets) {
      const auto requested_name =
          std::filesystem::path(utils::string::FromUtf8(asset.file.file_name)).filename().wstring();
      auto unique_name_result = make_unique_archive_name(requested_name, used_names);
      if (!unique_name_result) {
        return std::unexpected(unique_name_result.error());
      }
      asset.file.file_name = utils::string::ToUtf8(*unique_name_result);
    }

    // 多文件或部分成功都统一生成一次性 ZIP。
    auto token_result = create_archive(available_assets);
    if (!token_result) {
      return std::unexpected(token_result.error());
    }

    result.archive_token = *token_result;
    result.file_name = kArchiveFileName;
    return result;
  } catch (const std::exception& error) {
    return std::unexpected(std::string("Exception while preparing gallery download: ") +
                           error.what());
  }
}

// 将资产解析异常转换成 HTTP 层可处理的 expected 错误。
auto resolve_asset_file(core::AppState& app_state, std::int64_t asset_id)
    -> std::expected<DownloadFile, std::string> {
  try {
    return resolve_asset_file_impl(app_state, asset_id);
  } catch (const std::exception& error) {
    return std::unexpected(std::string("Exception while resolving gallery asset: ") + error.what());
  }
}

// 校验归档名和目录边界后返回临时 ZIP 的真实路径。
auto resolve_archive_file(core::AppState& app_state, std::string_view archive_name)
    -> std::expected<DownloadFile, std::string> {
  if (!is_safe_archive_name(archive_name)) {
    return std::unexpected("Invalid gallery download archive name");
  }

  auto directory_result = utils::path::GetAppDataSubdirectory(kDownloadDirectoryName);
  if (!directory_result) {
    return std::unexpected(directory_result.error());
  }

  // 客户端只能引用下载目录内、符合格式的归档文件。
  const auto archive_path = *directory_result / utils::string::FromUtf8(std::string(archive_name));
  if (!utils::path::IsPathWithinBase(archive_path, *directory_result) ||
      !is_regular_file_available(archive_path)) {
    return std::unexpected("Gallery download archive was not found");
  }

  return DownloadFile{
      .file_path = archive_path,
      .file_name = std::string(kArchiveFileName),
  };
}

// 完整传输后删除一次性归档，异常中断则留给下次启动清理。
auto remove_archive_file(const std::filesystem::path& archive_path) -> void {
  try {
    auto directory_result = utils::path::GetAppDataSubdirectory(kDownloadDirectoryName);
    if (!directory_result) {
      Logger().warn("Failed to locate gallery download directory after transfer: {}",
                    directory_result.error());
      return;
    }

    // 再次校验路径边界，避免完成回调成为任意文件删除入口。
    const auto archive_name = utils::string::ToUtf8(archive_path.filename().wstring());
    if (!is_safe_archive_name(archive_name) ||
        !utils::path::IsPathWithinBase(archive_path, *directory_result)) {
      Logger().warn("Refused to remove an unsafe gallery archive path: {}",
                    path_to_utf8(archive_path));
      return;
    }

    std::error_code remove_error;
    std::filesystem::remove(archive_path, remove_error);
    if (remove_error) {
      Logger().warn("Failed to remove gallery archive '{}': {}", path_to_utf8(archive_path),
                    remove_error.message());
    }
  } catch (const std::exception& error) {
    Logger().warn("Failed to remove gallery archive '{}': {}", path_to_utf8(archive_path),
                  error.what());
  }
}

}  // namespace features::gallery::download
