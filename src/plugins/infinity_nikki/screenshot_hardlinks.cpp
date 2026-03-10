module;

module Plugins.InfinityNikki.ScreenshotHardlinks;

import std;
import Core.State;
import Features.Settings.State;
import Plugins.InfinityNikki.Types;
import Utils.String;

namespace Plugins::InfinityNikki::ScreenshotHardlinks {

// 单次同步最多收集的错误条数，避免在大量文件出错时撑爆内存
constexpr std::size_t kMaxErrorMessages = 50;
// 游戏高清照片的存储路径结构：GamePlayPhotos/<uid>/NikkiPhotos_HighQuality/<文件名>
constexpr wchar_t kHighQualityFolderName[] = L"NikkiPhotos_HighQuality";
constexpr wchar_t kGamePlayPhotosFolderName[] = L"GamePlayPhotos";
// 游戏截图目录：本功能会将此目录管理为高清原图的硬链接镜像
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
  std::filesystem::path screenshot_dir;         // 放置硬链接镜像的目录
  std::filesystem::path screenshot_backup_dir;  // 原截图的备份目录
};

// 一张高清原图及其对应的硬链接信息
struct SourceAsset {
  std::filesystem::path target_path;  // 高清原图的绝对路径（硬链接目标）
  std::string uid;                    // 照片所属的 UID（路径第一级目录名）
  std::string original_filename;      // 原始文件名（不含路径）
  std::filesystem::path link_path;    // 将在 ScreenShot 目录中创建的硬链接路径
};

enum class LinkWriteAction {
  kNone,
  kCreated,
  kUpdated,
};

auto add_error(std::vector<std::string>& errors, std::string message) -> void {
  if (errors.size() >= kMaxErrorMessages) {
    return;
  }
  errors.push_back(std::move(message));
}

auto report_progress(
    const std::function<void(const InfinityNikkiInitializeScreenshotHardlinksProgress&)>&
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

  progress_callback(InfinityNikkiInitializeScreenshotHardlinksProgress{
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

auto make_name_with_suffix_preserving_extension(const std::string& filename, std::int32_t suffix)
    -> std::string {
  auto filename_path = std::filesystem::path(Utils::String::FromUtf8(filename));
  auto stem = Utils::String::ToUtf8(filename_path.stem().wstring());
  auto extension = Utils::String::ToUtf8(filename_path.extension().wstring());
  return std::format("{}_{}{}", stem, suffix, extension);
}

// 扫描 GamePlayPhotos 目录下所有高清原图，并为每张图计算唯一的硬链接文件名。
// 命名规则：若多张图文件名相同（来自不同 UID），则在文件名前加 "<uid>_" 前缀以区分；
// 若前缀后仍有冲突，则追加数字后缀 "_2"、"_3" 等（保留扩展名）
auto collect_source_assets(
    const ManagedPaths& paths,
    const std::function<void(const InfinityNikkiInitializeScreenshotHardlinksProgress&)>&
        progress_callback,
    InfinityNikkiInitializeScreenshotHardlinksResult& result) -> std::vector<SourceAsset> {
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

  // 为每个 source 分配全局唯一的硬链接文件名
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
      unique_name = make_name_with_suffix_preserving_extension(desired_name, suffix++);
      unique_key = Utils::String::ToLowerAscii(unique_name);
    }
    used_link_names.insert(unique_key);
    source.link_path =
        paths.screenshot_dir / std::filesystem::path(Utils::String::FromUtf8(unique_name));
  }

  // 按硬链接文件名排序，使游戏内截图列表顺序稳定
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
// 然后清空 ScreenShot 目录，为后续填入硬链接做准备。
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

auto are_equivalent_entries(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
    -> bool {
  std::error_code ec;
  auto equivalent = std::filesystem::equivalent(lhs, rhs, ec);
  return !ec && equivalent;
}

auto ensure_hardlink(const std::filesystem::path& link_path,
                     const std::filesystem::path& target_path)
    -> std::expected<LinkWriteAction, std::string> {
  if (make_path_compare_key(link_path) == make_path_compare_key(target_path)) {
    return std::unexpected("Managed hard link path must be different from target path: '" +
                           Utils::String::ToUtf8(link_path.wstring()) + "'");
  }

  auto ensure_result = ensure_directory_exists(link_path.parent_path());
  if (!ensure_result) {
    return std::unexpected(ensure_result.error());
  }

  std::error_code ec;
  auto link_exists = std::filesystem::exists(link_path, ec);
  if (ec) {
    return std::unexpected("Failed to inspect ScreenShot entry '" +
                           Utils::String::ToUtf8(link_path.wstring()) + "': " + ec.message());
  }

  if (link_exists && are_equivalent_entries(link_path, target_path)) {
    return LinkWriteAction::kNone;
  }

  auto action = link_exists ? LinkWriteAction::kUpdated : LinkWriteAction::kCreated;

  if (link_exists) {
    std::filesystem::remove_all(link_path, ec);
    if (ec) {
      return std::unexpected("Failed to replace existing ScreenShot entry '" +
                             Utils::String::ToUtf8(link_path.wstring()) + "': " + ec.message());
    }
  }

  std::filesystem::create_hard_link(target_path, link_path, ec);
  if (ec) {
    auto detail = ec.message();
    if (ec == std::make_error_code(std::errc::cross_device_link)) {
      detail += " (hard links require source and destination on the same volume)";
    }

    return std::unexpected("Failed to create hard link '" +
                           Utils::String::ToUtf8(link_path.wstring()) + "' -> '" +
                           Utils::String::ToUtf8(target_path.wstring()) + "': " + detail);
  }

  return action;
}

// 硬链接同步的核心逻辑，分两阶段执行：
// 1. 清理阶段：遍历 ScreenShot 目录，删除不在期望集合内的受管目录项
// 2. 同步阶段：遍历 sources，确保每个期望文件名都存在并指向目标原图
// backup_existing=true 时（首次初始化）先备份并清空 ScreenShot 目录
auto sync_hardlinks_internal(
    Core::State::AppState& app_state, bool backup_existing,
    const std::function<void(const InfinityNikkiInitializeScreenshotHardlinksProgress&)>&
        progress_callback)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string> {
  auto paths_result = resolve_managed_paths(app_state);
  if (!paths_result) {
    return std::unexpected(paths_result.error());
  }
  const auto paths = paths_result.value();

  InfinityNikkiInitializeScreenshotHardlinksResult result;

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

  // 以期望硬链接路径为键建立索引，用于清理阶段判断某个目录项是否应保留
  std::unordered_set<std::string> expected_link_keys;
  expected_link_keys.reserve(sources.size());
  for (const auto& source : sources) {
    expected_link_keys.insert(make_path_compare_key(source.link_path));
  }

  // 清理阶段：删除 ScreenShot 目录中不在期望集合内的内容
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(paths.screenshot_dir, ec)) {
    if (ec) {
      return std::unexpected("Failed to enumerate ScreenShot directory: " + ec.message());
    }

    auto entry_key = make_path_compare_key(entry.path());
    if (expected_link_keys.contains(entry_key)) {
      continue;
    }

    std::filesystem::remove_all(entry.path(), ec);
    if (!ec) {
      result.removed_count++;
    } else {
      add_error(result.errors, "Failed to remove obsolete ScreenShot entry '" +
                                   Utils::String::ToUtf8(entry.path().wstring()) +
                                   "': " + ec.message());
      ec.clear();
    }
  }

  // 同步阶段：确保每个 source 都有对应的硬链接
  for (std::size_t index = 0; index < sources.size(); ++index) {
    const auto& source = sources[index];
    report_progress(
        progress_callback, "syncing", static_cast<std::int64_t>(index), result.source_count,
        20.0 + (static_cast<double>(index) * 75.0 / std::max<std::int32_t>(result.source_count, 1)),
        std::format("Syncing {}", source.original_filename));

    auto ensure_result = ensure_hardlink(source.link_path, source.target_path);
    if (!ensure_result) {
      add_error(result.errors, ensure_result.error());
      continue;
    }

    switch (ensure_result.value()) {
      case LinkWriteAction::kCreated:
        result.created_count++;
        break;
      case LinkWriteAction::kUpdated:
        result.updated_count++;
        break;
      case LinkWriteAction::kNone:
      default:
        break;
    }
  }

  report_progress(
      progress_callback, "completed", result.source_count, result.source_count, 100.0,
      std::format("Done: {} created, {} updated, {} removed, {} ignored", result.created_count,
                  result.updated_count, result.removed_count, result.ignored_count));
  return result;
}

auto initialize(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeScreenshotHardlinksProgress&)>&
        progress_callback)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string> {
  return sync_hardlinks_internal(app_state, true, progress_callback);
}

auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeScreenshotHardlinksResult, std::string> {
  return sync_hardlinks_internal(app_state, false, nullptr);
}

auto resolve_watch_directory(Core::State::AppState& app_state)
    -> std::expected<std::filesystem::path, std::string> {
  auto paths_result = resolve_managed_paths(app_state);
  if (!paths_result) {
    return std::unexpected(paths_result.error());
  }
  return paths_result->gameplay_photos_dir;
}

}  // namespace Plugins::InfinityNikki::ScreenshotHardlinks
