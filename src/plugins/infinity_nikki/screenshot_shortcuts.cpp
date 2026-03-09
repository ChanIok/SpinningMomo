module;

#include <shlguid.h>
#include <shobjidl.h>
#include <wil/com.h>
#include <windows.h>

module Plugins.InfinityNikki.ScreenshotShortcuts;

import std;
import Core.State;
import Features.Settings.State;
import Plugins.InfinityNikki.Types;
import Utils.Logger;
import Utils.String;

namespace Plugins::InfinityNikki::ScreenshotShortcuts {

// 文件变更防抖延迟：游戏保存高清照片时会触发多次文件系统事件，延迟聚合后再同步
constexpr auto kWatchDebounceDelay = std::chrono::milliseconds(1200);
// 单次同步最多收集的错误条数，避免在大量文件出错时撑爆内存
constexpr std::size_t kMaxErrorMessages = 50;
// 游戏高清照片的存储路径结构：GamePlayPhotos/<uid>/NikkiPhotos_HighQuality/<文件名>
constexpr wchar_t kHighQualityFolderName[] = L"NikkiPhotos_HighQuality";
constexpr wchar_t kGamePlayPhotosFolderName[] = L"GamePlayPhotos";
// 游戏截图目录：本功能会将此目录清空并填入指向高清照片的快捷方式
constexpr wchar_t kScreenShotFolderName[] = L"ScreenShot";
// 首次初始化时，原 ScreenShot 目录的内容会备份至此
constexpr wchar_t kScreenShotBackupFolderName[] = L"ScreenShot_old";
constexpr std::array<std::string_view, 7> kSupportedExtensions = {".jpg",  ".jpeg", ".png", ".bmp",
                                                                  ".webp", ".tiff", ".tif"};

// 本功能管理的所有关键路径，由设置中的游戏目录派生而来
struct ManagedPaths {
  std::filesystem::path game_dir;
  std::filesystem::path x6game_dir;
  std::filesystem::path gameplay_photos_dir;    // 扫描高清原图的根目录
  std::filesystem::path screenshot_dir;         // 放置快捷方式的目录
  std::filesystem::path screenshot_backup_dir;  // 原截图的备份目录
};

// 一张高清原图及其对应的快捷方式信息
struct SourceAsset {
  std::filesystem::path target_path;  // 高清原图的绝对路径（快捷方式指向目标）
  std::string uid;                    // 照片所属的 UID（路径第一级目录名）
  std::string original_filename;      // 原始文件名（不含路径）
  std::filesystem::path link_path;    // 将在 ScreenShot 目录中创建的 .lnk 路径
};

// 文件监视服务的运行时状态，通过 service_runtime() 单例访问
struct ServiceRuntime {
  std::mutex mutex;
  std::jthread watch_thread;
  std::filesystem::path watch_root;  // 当前监视的目录，用于判断是否需要重启监视线程
};

// FindFirstChangeNotification 句柄的 RAII 包装，防止句柄泄漏
struct ChangeNotificationHandle {
  HANDLE handle = INVALID_HANDLE_VALUE;

  ChangeNotificationHandle() = default;
  explicit ChangeNotificationHandle(HANDLE raw_handle) : handle(raw_handle) {}
  ChangeNotificationHandle(const ChangeNotificationHandle&) = delete;
  auto operator=(const ChangeNotificationHandle&) -> ChangeNotificationHandle& = delete;
  ChangeNotificationHandle(ChangeNotificationHandle&& other) noexcept : handle(other.handle) {
    other.handle = INVALID_HANDLE_VALUE;
  }
  auto operator=(ChangeNotificationHandle&& other) noexcept -> ChangeNotificationHandle& {
    if (this == &other) {
      return *this;
    }
    close();
    handle = other.handle;
    other.handle = INVALID_HANDLE_VALUE;
    return *this;
  }
  ~ChangeNotificationHandle() { close(); }

  auto close() -> void {
    if (handle != INVALID_HANDLE_VALUE) {
      FindCloseChangeNotification(handle);
      handle = INVALID_HANDLE_VALUE;
    }
  }

  [[nodiscard]] auto is_valid() const -> bool { return handle != INVALID_HANDLE_VALUE; }
};

auto service_runtime() -> ServiceRuntime& {
  static ServiceRuntime runtime;
  return runtime;
}

auto add_error(std::vector<std::string>& errors, std::string message) -> void {
  if (errors.size() >= kMaxErrorMessages) {
    return;
  }
  errors.push_back(std::move(message));
}

auto report_progress(
    const std::function<void(const InfinityNikkiInitializeScreenshotShortcutsProgress&)>&
        progress_callback,
    std::string stage, std::int64_t current, std::int64_t total,
    std::optional<double> percent = std::nullopt, std::optional<std::string> message = std::nullopt)
    -> void {
  if (!progress_callback) {
    return;
  }

  if (percent.has_value()) {
    percent = std::clamp(*percent, 0.0, 100.0);
  }

  progress_callback(InfinityNikkiInitializeScreenshotShortcutsProgress{
      .stage = std::move(stage),
      .current = current,
      .total = total,
      .percent = percent,
      .message = std::move(message),
  });
}

// 路径规范化：若路径已存在则用 weakly_canonical 解析符号链接和 . / ..，
// 否则仅做词法规范化（不访问文件系统），确保路径可用于比较
auto normalize_existing_path(const std::filesystem::path& path) -> std::filesystem::path {
  std::error_code ec;
  if (std::filesystem::exists(path, ec) && !ec) {
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
      return normalized;
    }
  }
  return path.lexically_normal();
}

// 生成用于路径相等比较的规范化键：统一为正斜杠（generic）并转小写，
// 使路径比较在 Windows NTFS 上不区分大小写和路径分隔符
auto make_path_compare_key(const std::filesystem::path& path) -> std::string {
  auto normalized = normalize_existing_path(path).generic_wstring();
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return Utils::String::ToUtf8(normalized);
}

auto path_has_high_quality_segment(const std::filesystem::path& relative_path) -> bool {
  for (const auto& part : relative_path) {
    if (part.wstring() == kHighQualityFolderName) {
      return true;
    }
  }
  return false;
}

auto is_supported_image_extension(const std::filesystem::path& file_path) -> bool {
  auto extension = Utils::String::ToLowerAscii(file_path.extension().string());
  return std::ranges::any_of(kSupportedExtensions, [&extension](std::string_view candidate) {
    return extension == candidate;
  });
}

// 从高清照片路径提取 UID：路径结构为 GamePlayPhotos/<uid>/NikkiPhotos_HighQuality/...，
// 取相对路径的第一个目录名即为 UID
auto extract_uid_from_hq_path(const std::filesystem::path& gameplay_photos_dir,
                              const std::filesystem::path& file_path)
    -> std::optional<std::string> {
  std::error_code ec;
  auto relative_path = std::filesystem::relative(file_path, gameplay_photos_dir, ec);
  if (ec || relative_path.empty()) {
    return std::nullopt;
  }

  return Utils::String::ToUtf8(relative_path.begin()->wstring());
}

// 判断文件是否为本功能管理的高清原图：需满足普通文件、图片扩展名、
// 且路径中包含 NikkiPhotos_HighQuality 目录段
auto is_hq_photo_file(const ManagedPaths& paths, const std::filesystem::path& file_path) -> bool {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file_path, ec) || ec) {
    return false;
  }

  if (!is_supported_image_extension(file_path)) {
    return false;
  }

  auto relative_path = std::filesystem::relative(file_path, paths.gameplay_photos_dir, ec);
  if (ec || relative_path.empty()) {
    return false;
  }

  return path_has_high_quality_segment(relative_path);
}

// 从设置中的游戏目录派生所有受管路径，并验证 GamePlayPhotos 目录实际存在
auto resolve_managed_paths(Core::State::AppState& app_state)
    -> std::expected<ManagedPaths, std::string> {
  if (!app_state.settings) {
    return std::unexpected("Settings state is not initialized");
  }

  const auto& game_dir_utf8 = app_state.settings->raw.plugins.infinity_nikki.game_dir;
  if (game_dir_utf8.empty()) {
    return std::unexpected("Infinity Nikki game directory is empty");
  }

  auto game_dir = std::filesystem::path(Utils::String::FromUtf8(game_dir_utf8));
  if (game_dir.empty()) {
    return std::unexpected("Failed to resolve Infinity Nikki game directory");
  }

  auto x6game_dir = game_dir / L"X6Game";
  auto gameplay_photos_dir = x6game_dir / L"Saved" / kGamePlayPhotosFolderName;
  auto screenshot_dir = x6game_dir / kScreenShotFolderName;
  auto screenshot_backup_dir = x6game_dir / kScreenShotBackupFolderName;

  std::error_code ec;
  if (!std::filesystem::exists(gameplay_photos_dir, ec) || ec) {
    return std::unexpected("GamePlayPhotos directory does not exist");
  }

  // screenshot_dir 和 backup_dir 在此阶段可能尚不存在，仅做词法规范化
  return ManagedPaths{
      .game_dir = normalize_existing_path(game_dir),
      .x6game_dir = normalize_existing_path(x6game_dir),
      .gameplay_photos_dir = normalize_existing_path(gameplay_photos_dir),
      .screenshot_dir = screenshot_dir.lexically_normal(),
      .screenshot_backup_dir = screenshot_backup_dir.lexically_normal(),
  };
}

// 扫描 GamePlayPhotos 目录下所有高清原图，并为每张图计算唯一的 .lnk 快捷方式名称。
// 命名规则：若多张图文件名相同（来自不同 UID），则在文件名前加 "<uid>_" 前缀以区分；
// 若前缀后仍有冲突，则追加数字后缀 "_2"、"_3" 等
auto collect_source_assets(
    const ManagedPaths& paths,
    const std::function<void(const InfinityNikkiInitializeScreenshotShortcutsProgress&)>&
        progress_callback,
    InfinityNikkiInitializeScreenshotShortcutsResult& result) -> std::vector<SourceAsset> {
  std::vector<SourceAsset> sources;
  std::error_code ec;

  report_progress(progress_callback, "preparing", 0, 0, 5.0, "Scanning NikkiPhotos_HighQuality");

  std::filesystem::recursive_directory_iterator end_iter;
  for (std::filesystem::recursive_directory_iterator iter(
           paths.gameplay_photos_dir, std::filesystem::directory_options::skip_permission_denied,
           ec);
       iter != end_iter; iter.increment(ec)) {
    if (ec) {
      add_error(result.errors, "Failed to iterate GamePlayPhotos: " + ec.message());
      ec.clear();
      continue;
    }

    if (!iter->is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }

    auto file_path = iter->path();
    if (!is_hq_photo_file(paths, file_path)) {
      continue;
    }

    auto uid = extract_uid_from_hq_path(paths.gameplay_photos_dir, file_path);
    if (!uid.has_value()) {
      add_error(result.errors,
                "Failed to parse UID from path: " + Utils::String::ToUtf8(file_path.wstring()));
      continue;
    }

    sources.push_back(SourceAsset{
        .target_path = normalize_existing_path(file_path),
        .uid = std::move(uid.value()),
        .original_filename = Utils::String::ToUtf8(file_path.filename().wstring()),
    });
  }

  // 统计各文件名出现次数（不区分大小写），用于判断是否需要加 UID 前缀
  std::unordered_map<std::string, std::int32_t> filename_counts;
  filename_counts.reserve(sources.size());
  for (const auto& source : sources) {
    auto key = Utils::String::ToLowerAscii(source.original_filename);
    filename_counts[key]++;
  }

  // 为每个 source 分配全局唯一的快捷方式名称
  std::unordered_set<std::string> used_link_names;
  used_link_names.reserve(sources.size());
  for (auto& source : sources) {
    auto desired_name = source.original_filename;
    auto lower_name = Utils::String::ToLowerAscii(desired_name);
    if (filename_counts[lower_name] > 1) {
      desired_name = source.uid + "_" + source.original_filename;
    }

    auto unique_name = desired_name;
    auto unique_key = Utils::String::ToLowerAscii(unique_name);
    std::int32_t suffix = 2;
    while (used_link_names.contains(unique_key)) {
      unique_name = std::format("{}_{}", desired_name, suffix++);
      unique_key = Utils::String::ToLowerAscii(unique_name);
    }
    used_link_names.insert(unique_key);
    source.link_path =
        paths.screenshot_dir / std::filesystem::path(Utils::String::FromUtf8(unique_name + ".lnk"));
  }

  // 按快捷方式文件名排序，使游戏内截图列表顺序稳定
  std::ranges::sort(sources, [](const SourceAsset& lhs, const SourceAsset& rhs) {
    return lhs.link_path.filename().wstring() < rhs.link_path.filename().wstring();
  });

  result.source_count = static_cast<std::int32_t>(sources.size());
  return sources;
}

auto ensure_directory_exists(const std::filesystem::path& path)
    -> std::expected<void, std::string> {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    return std::unexpected("Failed to create directory '" + Utils::String::ToUtf8(path.wstring()) +
                           "': " + ec.message());
  }
  return {};
}

// 将 source_path（文件或目录）递归复制到 backup_path，
// 目录情况下保留内部结构，文件情况下直接覆盖复制
auto copy_entry_into_backup(const std::filesystem::path& source_path,
                            const std::filesystem::path& backup_path)
    -> std::expected<void, std::string> {
  std::error_code ec;
  if (std::filesystem::is_directory(source_path, ec)) {
    if (ec) {
      return std::unexpected("Failed to inspect directory '" +
                             Utils::String::ToUtf8(source_path.wstring()) + "': " + ec.message());
    }

    std::filesystem::create_directories(backup_path, ec);
    if (ec) {
      return std::unexpected("Failed to create backup directory '" +
                             Utils::String::ToUtf8(backup_path.wstring()) + "': " + ec.message());
    }

    std::filesystem::recursive_directory_iterator end_iter;
    for (std::filesystem::recursive_directory_iterator iter(
             source_path, std::filesystem::directory_options::skip_permission_denied, ec);
         iter != end_iter; iter.increment(ec)) {
      if (ec) {
        return std::unexpected("Failed to enumerate backup source '" +
                               Utils::String::ToUtf8(source_path.wstring()) + "': " + ec.message());
      }

      auto relative_path = std::filesystem::relative(iter->path(), source_path, ec);
      if (ec) {
        return std::unexpected("Failed to build backup relative path for '" +
                               Utils::String::ToUtf8(iter->path().wstring()) +
                               "': " + ec.message());
      }

      auto destination_path = backup_path / relative_path;
      if (iter->is_directory(ec)) {
        ec.clear();
        std::filesystem::create_directories(destination_path, ec);
        if (ec) {
          return std::unexpected("Failed to create backup directory '" +
                                 Utils::String::ToUtf8(destination_path.wstring()) +
                                 "': " + ec.message());
        }
        continue;
      }

      if (!iter->is_regular_file(ec) || ec) {
        ec.clear();
        continue;
      }

      std::filesystem::create_directories(destination_path.parent_path(), ec);
      if (ec) {
        return std::unexpected("Failed to create parent directory '" +
                               Utils::String::ToUtf8(destination_path.parent_path().wstring()) +
                               "': " + ec.message());
      }
      std::filesystem::copy_file(iter->path(), destination_path,
                                 std::filesystem::copy_options::overwrite_existing, ec);
      if (ec) {
        return std::unexpected("Failed to backup file '" +
                               Utils::String::ToUtf8(iter->path().wstring()) +
                               "': " + ec.message());
      }
    }

    return {};
  }

  if (!std::filesystem::is_regular_file(source_path, ec) || ec) {
    return {};
  }

  std::filesystem::create_directories(backup_path.parent_path(), ec);
  if (ec) {
    return std::unexpected("Failed to create parent directory '" +
                           Utils::String::ToUtf8(backup_path.parent_path().wstring()) +
                           "': " + ec.message());
  }
  std::filesystem::copy_file(source_path, backup_path,
                             std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    return std::unexpected("Failed to backup file '" +
                           Utils::String::ToUtf8(source_path.wstring()) + "': " + ec.message());
  }

  return {};
}

// 首次初始化时调用：将现有 ScreenShot 目录的全部内容备份至 ScreenShot_old，
// 然后清空 ScreenShot 目录，为后续填入快捷方式做准备。
// 若 ScreenShot 目录尚不存在，则直接创建空目录
auto backup_existing_screenshot_directory(const ManagedPaths& paths)
    -> std::expected<void, std::string> {
  std::error_code ec;
  if (!std::filesystem::exists(paths.screenshot_dir, ec) || ec) {
    ec.clear();
    return ensure_directory_exists(paths.screenshot_dir);
  }

  auto ensure_result = ensure_directory_exists(paths.screenshot_backup_dir);
  if (!ensure_result) {
    return ensure_result;
  }

  for (const auto& entry : std::filesystem::directory_iterator(paths.screenshot_dir, ec)) {
    if (ec) {
      return std::unexpected("Failed to enumerate ScreenShot directory: " + ec.message());
    }

    auto backup_target = paths.screenshot_backup_dir / entry.path().filename();
    auto backup_result = copy_entry_into_backup(entry.path(), backup_target);
    if (!backup_result) {
      return backup_result;
    }
  }

  std::filesystem::remove_all(paths.screenshot_dir, ec);
  if (ec) {
    return std::unexpected("Failed to clear ScreenShot directory: " + ec.message());
  }

  return ensure_directory_exists(paths.screenshot_dir);
}

// 通过 COM IShellLink 创建 Windows 快捷方式（.lnk）。
// 先删除同名旧文件（忽略失败），再重新创建，确保快捷方式内容始终是最新的
auto create_shortcut(const std::filesystem::path& shortcut_path,
                     const std::filesystem::path& target_path) -> std::expected<void, std::string> {
  auto ensure_result = ensure_directory_exists(shortcut_path.parent_path());
  if (!ensure_result) {
    return ensure_result;
  }

  std::error_code ec;
  std::filesystem::remove(shortcut_path, ec);
  ec.clear();

  wil::com_ptr<IShellLinkW> shell_link;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(shell_link.put()));
  if (FAILED(hr)) {
    return std::unexpected("Failed to create ShellLink COM instance");
  }

  auto target_w = target_path.wstring();
  hr = shell_link->SetPath(target_w.c_str());
  if (FAILED(hr)) {
    return std::unexpected("Failed to set shortcut target path");
  }

  auto workdir_w = target_path.parent_path().wstring();
  hr = shell_link->SetWorkingDirectory(workdir_w.c_str());
  if (FAILED(hr)) {
    return std::unexpected("Failed to set shortcut working directory");
  }

  shell_link->SetDescription(L"SpinningMomo Infinity Nikki shortcut");

  wil::com_ptr<IPersistFile> persist_file = shell_link.query<IPersistFile>();
  if (!persist_file) {
    return std::unexpected("Failed to acquire IPersistFile from ShellLink");
  }

  auto shortcut_w = shortcut_path.wstring();
  hr = persist_file->Save(shortcut_w.c_str(), TRUE);
  if (FAILED(hr)) {
    return std::unexpected("Failed to save shortcut file");
  }

  return {};
}

// 通过 COM IShellLink 读取 .lnk 快捷方式的指向目标路径
auto resolve_shortcut_target(const std::filesystem::path& shortcut_path)
    -> std::expected<std::filesystem::path, std::string> {
  wil::com_ptr<IShellLinkW> shell_link;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(shell_link.put()));
  if (FAILED(hr)) {
    return std::unexpected("Failed to create ShellLink COM instance");
  }

  wil::com_ptr<IPersistFile> persist_file = shell_link.query<IPersistFile>();
  if (!persist_file) {
    return std::unexpected("Failed to acquire IPersistFile from ShellLink");
  }

  auto shortcut_w = shortcut_path.wstring();
  hr = persist_file->Load(shortcut_w.c_str(), STGM_READ);
  if (FAILED(hr)) {
    return std::unexpected("Failed to load shortcut file");
  }

  std::array<wchar_t, 32768> path_buffer{};
  WIN32_FIND_DATAW find_data{};
  hr = shell_link->GetPath(path_buffer.data(), static_cast<int>(path_buffer.size()), &find_data,
                           SLGP_RAWPATH);
  if (FAILED(hr) || path_buffer[0] == L'\0') {
    return std::unexpected("Failed to resolve shortcut target path");
  }

  return normalize_existing_path(std::filesystem::path(path_buffer.data()));
}

// 快捷方式同步的核心逻辑，分两阶段执行：
// 1. 清理阶段：遍历 ScreenShot 目录，删除已失效或路径不符的旧快捷方式
// 2. 同步阶段：遍历 sources，跳过已正确的快捷方式，更新指向变化的，创建缺失的
// backup_existing=true 时（首次初始化）先备份并清空 ScreenShot 目录
auto sync_shortcuts_internal(
    Core::State::AppState& app_state, bool backup_existing,
    const std::function<void(const InfinityNikkiInitializeScreenshotShortcutsProgress&)>&
        progress_callback)
    -> std::expected<InfinityNikkiInitializeScreenshotShortcutsResult, std::string> {
  auto paths_result = resolve_managed_paths(app_state);
  if (!paths_result) {
    return std::unexpected(paths_result.error());
  }
  const auto paths = paths_result.value();

  InfinityNikkiInitializeScreenshotShortcutsResult result;

  // COM 快捷方式操作需要在 STA 线程中进行
  auto co_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
  (void)co_init;

  if (backup_existing) {
    report_progress(progress_callback, "preparing", 0, 0, 10.0, "Backing up ScreenShot directory");
    auto backup_result = backup_existing_screenshot_directory(paths);
    if (!backup_result) {
      return std::unexpected(backup_result.error());
    }
  } else {
    auto ensure_result = ensure_directory_exists(paths.screenshot_dir);
    if (!ensure_result) {
      return std::unexpected(ensure_result.error());
    }
  }

  auto sources = collect_source_assets(paths, progress_callback, result);
  report_progress(progress_callback, "syncing", 0, result.source_count, 20.0,
                  std::format("Found {} high-quality photos", result.source_count));

  // 以目标路径为键建立索引，用于清理阶段快速查找某个 .lnk 是否仍然有效
  std::unordered_map<std::string, SourceAsset> sources_by_target_key;
  sources_by_target_key.reserve(sources.size());
  for (auto& source : sources) {
    sources_by_target_key.emplace(make_path_compare_key(source.target_path), source);
  }

  // 清理阶段：删除 ScreenShot 目录中指向高清原图但已过期的快捷方式
  // （目标文件已不存在，或快捷方式文件名与预期不符）
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(paths.screenshot_dir, ec)) {
    if (ec) {
      return std::unexpected("Failed to enumerate ScreenShot directory: " + ec.message());
    }

    if (!entry.is_regular_file(ec) || ec) {
      ec.clear();
      continue;
    }

    auto entry_path = entry.path();
    if (Utils::String::ToLowerAscii(entry_path.extension().string()) != ".lnk") {
      continue;
    }

    auto target_result = resolve_shortcut_target(entry_path);
    if (!target_result) {
      continue;
    }

    auto target_path = target_result.value();
    if (!is_hq_photo_file(paths, target_path)) {
      // 不是本功能管理的快捷方式，跳过，保留用户自行创建的内容
      continue;
    }

    auto target_key = make_path_compare_key(target_path);
    auto expected_it = sources_by_target_key.find(target_key);
    if (expected_it == sources_by_target_key.end() ||
        make_path_compare_key(expected_it->second.link_path) != make_path_compare_key(entry_path)) {
      std::filesystem::remove(entry_path, ec);
      if (!ec) {
        result.removed_count++;
      } else {
        add_error(result.errors, "Failed to remove obsolete shortcut '" +
                                     Utils::String::ToUtf8(entry_path.wstring()) +
                                     "': " + ec.message());
        ec.clear();
      }
    }
  }

  // 同步阶段：确保每个 source 都有对应的、指向正确目标的快捷方式
  for (std::size_t index = 0; index < sources.size(); ++index) {
    const auto& source = sources[index];
    report_progress(
        progress_callback, "syncing", static_cast<std::int64_t>(index), result.source_count,
        20.0 + (static_cast<double>(index) * 75.0 / std::max<std::int32_t>(result.source_count, 1)),
        std::format("Syncing {}", source.original_filename));

    std::error_code exists_ec;
    auto link_exists = std::filesystem::exists(source.link_path, exists_ec) && !exists_ec;
    if (link_exists) {
      if (!std::filesystem::is_regular_file(source.link_path, exists_ec) || exists_ec) {
        // 同名路径存在但不是普通文件（如目录），跳过以免破坏用户数据
        result.ignored_count++;
        exists_ec.clear();
        continue;
      }

      auto target_result = resolve_shortcut_target(source.link_path);
      if (target_result) {
        if (make_path_compare_key(target_result.value()) ==
            make_path_compare_key(source.target_path)) {
          // 快捷方式已正确指向目标，无需变更
          continue;
        }

        if (!is_hq_photo_file(paths, target_result.value())) {
          // 同名 .lnk 指向的是非高清原图（用户自行创建的快捷方式），跳过
          result.ignored_count++;
          continue;
        }
      }

      // 快捷方式指向的目标已变化，重新创建
      auto recreate_result = create_shortcut(source.link_path, source.target_path);
      if (!recreate_result) {
        add_error(result.errors, recreate_result.error());
        continue;
      }
      result.updated_count++;
      continue;
    }

    auto create_result = create_shortcut(source.link_path, source.target_path);
    if (!create_result) {
      add_error(result.errors, create_result.error());
      continue;
    }
    result.created_count++;
  }

  report_progress(
      progress_callback, "completed", result.source_count, result.source_count, 100.0,
      std::format("Done: {} created, {} updated, {} removed, {} ignored", result.created_count,
                  result.updated_count, result.removed_count, result.ignored_count));
  return result;
}

// 文件监视循环：监听 GamePlayPhotos 目录下的文件变化，
// 触发变化后等待防抖延迟（kWatchDebounceDelay）才执行同步，
// 避免游戏批量写入时反复触发。启动时 pending_sync=true，
// 确保监视线程启动后即执行一次同步，补偿监视期间错过的变更
auto watch_loop(std::stop_token stop_token, Core::State::AppState* app_state,
                std::filesystem::path watch_root) -> void {
  if (!app_state) {
    return;
  }

  auto co_init = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
  (void)co_init;

  auto handle = ChangeNotificationHandle(
      FindFirstChangeNotificationW(watch_root.wstring().c_str(), TRUE,
                                   FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                       FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE));
  if (!handle.is_valid()) {
    Logger().warn("Infinity Nikki screenshot shortcut watcher failed to start: {}",
                  Utils::String::ToUtf8(watch_root.wstring()));
    return;
  }

  Logger().info("Infinity Nikki screenshot shortcut watcher started: {}",
                Utils::String::ToUtf8(watch_root.wstring()));

  auto last_change = std::chrono::steady_clock::now();
  bool pending_sync = true;

  while (!stop_token.stop_requested()) {
    // 250ms 轮询间隔，兼顾响应性与 CPU 占用
    auto wait_result = WaitForSingleObject(handle.handle, 250);
    if (wait_result == WAIT_OBJECT_0) {
      last_change = std::chrono::steady_clock::now();
      pending_sync = true;
      if (!FindNextChangeNotification(handle.handle)) {
        Logger().warn("Infinity Nikki screenshot shortcut watcher lost notifications: {}",
                      Utils::String::ToUtf8(watch_root.wstring()));
        break;
      }
      continue;
    }

    if (wait_result == WAIT_FAILED) {
      Logger().warn("Infinity Nikki screenshot shortcut watcher wait failed: {}",
                    Utils::String::ToUtf8(watch_root.wstring()));
      break;
    }

    if (!pending_sync) {
      continue;
    }

    if (std::chrono::steady_clock::now() - last_change < kWatchDebounceDelay) {
      continue;
    }

    pending_sync = false;
    auto sync_result = sync(*app_state);
    if (!sync_result) {
      Logger().warn("Infinity Nikki screenshot shortcut sync failed: {}", sync_result.error());
      continue;
    }

    Logger().info(
        "Infinity Nikki screenshot shortcut sync completed: source={}, created={}, updated={}, "
        "removed={}, ignored={}",
        sync_result->source_count, sync_result->created_count, sync_result->updated_count,
        sync_result->removed_count, sync_result->ignored_count);
  }

  Logger().info("Infinity Nikki screenshot shortcut watcher stopped: {}",
                Utils::String::ToUtf8(watch_root.wstring()));
}

auto stop_watch_thread(ServiceRuntime& runtime) -> void {
  if (!runtime.watch_thread.joinable()) {
    runtime.watch_root.clear();
    return;
  }

  runtime.watch_thread.request_stop();
  runtime.watch_thread.join();
  runtime.watch_root.clear();
}

auto initialize(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeScreenshotShortcutsProgress&)>&
        progress_callback)
    -> std::expected<InfinityNikkiInitializeScreenshotShortcutsResult, std::string> {
  return sync_shortcuts_internal(app_state, true, progress_callback);
}

auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeScreenshotShortcutsResult, std::string> {
  return sync_shortcuts_internal(app_state, false, nullptr);
}

// 根据当前设置决定是否启动/重启/停止文件监视服务。
// 若监视目录未变化，则保持现有监视线程不变，避免不必要的重启
auto refresh_from_settings(Core::State::AppState& app_state) -> void {
  auto& runtime = service_runtime();
  std::lock_guard<std::mutex> lock(runtime.mutex);

  if (!app_state.settings) {
    stop_watch_thread(runtime);
    return;
  }

  const auto& config = app_state.settings->raw.plugins.infinity_nikki;
  if (!config.enable || !config.manage_screenshot_shortcuts || config.game_dir.empty()) {
    stop_watch_thread(runtime);
    return;
  }

  auto paths_result = resolve_managed_paths(app_state);
  if (!paths_result) {
    Logger().warn("Skip Infinity Nikki screenshot shortcut watcher: {}", paths_result.error());
    stop_watch_thread(runtime);
    return;
  }

  auto watch_root = paths_result->gameplay_photos_dir;
  if (runtime.watch_thread.joinable() &&
      make_path_compare_key(runtime.watch_root) == make_path_compare_key(watch_root)) {
    return;
  }

  stop_watch_thread(runtime);
  runtime.watch_root = watch_root;
  runtime.watch_thread = std::jthread(watch_loop, &app_state, watch_root);
}

auto shutdown() -> void {
  auto& runtime = service_runtime();
  std::lock_guard<std::mutex> lock(runtime.mutex);
  stop_watch_thread(runtime);
}

}  // namespace Plugins::InfinityNikki::ScreenshotShortcuts
