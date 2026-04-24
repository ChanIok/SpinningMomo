module;

module Extensions.InfinityNikki.MediaHardlinks;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Watcher;
import Features.Settings.State;
import Extensions.InfinityNikki.Types;
import Utils.Logger;
import Utils.String;
import Utils.System;

namespace Extensions::InfinityNikki::MediaHardlinks {

// 设计目标：
// 1) 把 GamePlayPhotos 下的照片/录像投影到 X6Game/ScreenShot 与 X6Game/Video；
// 2) 投影项使用硬链接，避免重复占用磁盘；
// 3) 同时支持全量重建（initialize/sync）和运行时增量（apply_runtime_changes）。
constexpr std::size_t kMaxErrorMessages = 50;
constexpr wchar_t kHighQualityFolderName[] = L"NikkiPhotos_HighQuality";
constexpr wchar_t kVideosFolderName[] = L"Videos";
constexpr wchar_t kGamePlayPhotosFolderName[] = L"GamePlayPhotos";
constexpr wchar_t kScreenShotFolderName[] = L"ScreenShot";
constexpr wchar_t kVideoFolderName[] = L"Video";
constexpr std::string_view kManagedVideoExtension = ".mp4";
constexpr std::array<std::string_view, 7> kSupportedImageExtensions = {
    ".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tiff", ".tif"};

// 运行时路径快照：
// - gameplay_photos_dir：真实来源（游戏拍照/录像输出）
// - screenshot_dir/video_dir：我们维护的“投影目录”（里面是硬链接）
// 注意：这里不保存数据库状态，只是本次同步需要的路径集合。
struct ManagedPaths {
  std::filesystem::path game_dir;
  std::filesystem::path x6game_dir;
  std::filesystem::path gameplay_photos_dir;
  std::filesystem::path screenshot_dir;
  std::filesystem::path video_dir;
};

enum class ManagedSourceKind {
  kPhoto,
  kVideo,
};

struct ManagedSource {
  // kind 决定后续删除阶段是否要额外清理录像 session 目录。
  ManagedSourceKind kind = ManagedSourceKind::kPhoto;
  // target_path：真实源文件路径（硬链接指向它）
  std::filesystem::path target_path;
  // link_path：投影目录下的链接路径（被创建/更新/删除的对象）
  std::filesystem::path link_path;
  // original_filename：仅用于日志/进度提示，减少反复转换编码。
  std::string original_filename;
  // 仅录像使用：记录 session 目录，便于 REMOVE 时一起回收。
  std::optional<std::filesystem::path> session_directory;
};

enum class LinkWriteAction {
  kNone,
  kCreated,
  kUpdated,
};

// 追加错误信息（带上限），避免长任务错误列表无限增长。
auto add_error(std::vector<std::string>& errors, std::string message) -> void {
  // 错误数量封顶：避免极端场景（几十万文件）下错误列表无限增长导致内存膨胀。
  if (errors.size() >= kMaxErrorMessages) {
    return;
  }
  errors.push_back(std::move(message));
}

// 安全上报任务进度：支持空回调，并保证 percent 在 0~100。
auto report_progress(
    const std::function<void(const InfinityNikkiInitializeMediaHardlinksProgress&)>&
        progress_callback,
    std::string stage, std::int64_t current, std::int64_t total,
    std::optional<double> percent = std::nullopt, std::optional<std::string> message = std::nullopt)
    -> void {
  // 进度回调是可选的：静默模式（sync）会传 nullptr。
  if (!progress_callback) {
    return;
  }

  if (percent.has_value()) {
    percent = std::clamp(*percent, 0.0, 100.0);
  }

  progress_callback(InfinityNikkiInitializeMediaHardlinksProgress{
      .stage = std::move(stage),
      .current = current,
      .total = total,
      .percent = percent,
      .message = std::move(message),
  });
}

// 尽可能把路径规范化为稳定形式，便于后续比较与去重。
auto normalize_existing_path(const std::filesystem::path& path) -> std::filesystem::path {
  // 优先 weakly_canonical：尽量得到稳定、可比较的绝对路径；
  // 若路径不存在/规范化失败，则退回 lexically_normal，保证函数总是有返回值。
  std::error_code ec;
  if (std::filesystem::exists(path, ec) && !ec) {
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
      return normalized;
    }
  }
  return path.lexically_normal();
}

// 生成路径比较键（规范化 + 小写），用于大小写不敏感环境下的稳定比较。
auto make_path_compare_key(const std::filesystem::path& path) -> std::string {
  // Windows 文件系统大小写不敏感；这里统一做 canonical + lower，避免“同一路径不同写法”误判。
  auto normalized = normalize_existing_path(path).generic_wstring();
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return Utils::String::ToUtf8(normalized);
}

// 判断路径片段是否为纯数字（用于 UID 目录判定）。
auto is_numeric_path_part(const std::filesystem::path& part) -> bool {
  auto value = part.wstring();
  return !value.empty() &&
         std::ranges::all_of(value, [](wchar_t ch) { return std::iswdigit(ch) != 0; });
}

// 判断是否为受支持的图片扩展名。
auto is_supported_image_extension(const std::filesystem::path& file_path) -> bool {
  // 扩展名比较统一小写，避免 .JPG / .Png 之类大小写差异漏判。
  auto extension = Utils::String::ToLowerAscii(file_path.extension().string());
  return std::ranges::any_of(kSupportedImageExtensions, [&extension](std::string_view candidate) {
    return extension == candidate;
  });
}

// 判断是否为受管视频扩展名（当前仅 .mp4）。
auto is_supported_managed_video_extension(const std::filesystem::path& file_path) -> bool {
  return Utils::String::ToLowerAscii(file_path.extension().string()) == kManagedVideoExtension;
}

// 从设置解析并校验 Infinity Nikki 源目录与硬链接投影目录。
auto resolve_managed_paths(Core::State::AppState& app_state)
    -> std::expected<ManagedPaths, std::string> {
  // 这一步只做“路径解析与存在性校验”，不做任何写盘动作。
  if (!app_state.settings) {
    return std::unexpected("Settings state is not initialized");
  }

  const auto& game_dir_utf8 = app_state.settings->raw.extensions.infinity_nikki.game_dir;
  if (game_dir_utf8.empty()) {
    return std::unexpected("Infinity Nikki game directory is empty");
  }

  auto game_dir = std::filesystem::path(Utils::String::FromUtf8(game_dir_utf8));
  if (game_dir.empty()) {
    return std::unexpected("Failed to resolve Infinity Nikki game directory");
  }

  // 来源目录：X6Game/Saved/GamePlayPhotos
  // 投影目录：X6Game/ScreenShot（照片） + X6Game/Video（录像）
  auto x6game_dir = game_dir / L"X6Game";
  auto gameplay_photos_dir = x6game_dir / L"Saved" / kGamePlayPhotosFolderName;
  auto screenshot_dir = x6game_dir / kScreenShotFolderName;
  auto video_dir = x6game_dir / kVideoFolderName;

  std::error_code ec;
  if (!std::filesystem::exists(gameplay_photos_dir, ec) || ec) {
    return std::unexpected("GamePlayPhotos directory does not exist");
  }

  return ManagedPaths{
      .game_dir = normalize_existing_path(game_dir),
      .x6game_dir = normalize_existing_path(x6game_dir),
      .gameplay_photos_dir = normalize_existing_path(gameplay_photos_dir),
      .screenshot_dir = screenshot_dir.lexically_normal(),
      .video_dir = video_dir.lexically_normal(),
  };
}

// 计算照片在 ScreenShot 投影目录中的硬链接路径。
auto make_photo_link_path(const ManagedPaths& paths, const std::filesystem::path& target_path)
    -> std::filesystem::path {
  // 照片投影策略：直接平铺到 ScreenShot，文件名与源文件保持一致。
  return paths.screenshot_dir / target_path.filename();
}

// 计算录像在 Video 投影目录中的硬链接路径。
auto make_video_link_path(const ManagedPaths& paths, const std::filesystem::path& target_path)
    -> std::filesystem::path {
  // 录像投影策略：直接平铺到 Video，文件名与源文件保持一致。
  return paths.video_dir / target_path.filename();
}

// 尝试把任意文件识别为“受管照片源”，失败返回 nullopt。
auto try_make_photo_source(const ManagedPaths& paths, const std::filesystem::path& file_path,
                           bool require_existing_file) -> std::optional<ManagedSource> {
  // try_make_* 系列是“识别器”：
  // - 命中规则 -> 返回 ManagedSource
  // - 不命中 -> 返回 nullopt（不是错误）
  if (!is_supported_image_extension(file_path)) {
    return std::nullopt;
  }

  if (require_existing_file) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file_path, ec) || ec) {
      return std::nullopt;
    }
  }

  auto relative_path = file_path.lexically_relative(paths.gameplay_photos_dir);
  if (relative_path.empty()) {
    return std::nullopt;
  }

  // 照片来源判定规则：位于 GamePlayPhotos 相对路径中，且路径包含 NikkiPhotos_HighQuality 段。
  bool has_hq_segment = false;
  for (const auto& part : relative_path) {
    if (part.wstring() == kHighQualityFolderName) {
      has_hq_segment = true;
      break;
    }
  }

  if (!has_hq_segment) {
    return std::nullopt;
  }

  auto normalized_target = normalize_existing_path(file_path);
  return ManagedSource{
      .kind = ManagedSourceKind::kPhoto,
      .target_path = normalized_target,
      .link_path = make_photo_link_path(paths, normalized_target),
      .original_filename = Utils::String::ToUtf8(normalized_target.filename().wstring()),
      .session_directory = std::nullopt,
  };
}

// 尝试把任意文件识别为“受管录像源”，失败返回 nullopt。
auto try_make_video_source(const ManagedPaths& paths, const std::filesystem::path& file_path,
                           bool require_existing_file) -> std::optional<ManagedSource> {
  if (!is_supported_managed_video_extension(file_path)) {
    return std::nullopt;
  }

  if (require_existing_file) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file_path, ec) || ec) {
      return std::nullopt;
    }
  }

  auto relative_path = file_path.lexically_relative(paths.gameplay_photos_dir);
  if (relative_path.empty()) {
    return std::nullopt;
  }

  // 录像来源判定规则：
  // GamePlayPhotos/<uid>/Videos/<session>/<session>.mp4
  // 仅该命名结构视为“受管录像”，避免误处理其它 mp4。
  std::vector<std::filesystem::path> parts;
  for (const auto& part : relative_path) {
    parts.push_back(part);
  }

  if (parts.size() != 4) {
    // 不是标准层级，直接忽略。
    return std::nullopt;
  }

  if (!is_numeric_path_part(parts[0]) || parts[1].wstring() != kVideosFolderName) {
    return std::nullopt;
  }

  auto session_name = Utils::String::ToUtf8(parts[2].wstring());
  auto expected_filename =
      Utils::String::ToLowerAscii(session_name + std::string(kManagedVideoExtension));
  auto actual_filename = Utils::String::ToLowerAscii(parts[3].string());
  if (actual_filename != expected_filename) {
    return std::nullopt;
  }

  auto normalized_target = normalize_existing_path(file_path);
  return ManagedSource{
      .kind = ManagedSourceKind::kVideo,
      .target_path = normalized_target,
      .link_path = make_video_link_path(paths, normalized_target),
      .original_filename = Utils::String::ToUtf8(normalized_target.filename().wstring()),
      .session_directory = normalized_target.parent_path(),
  };
}

// 统一尝试照片/录像规则，命中其一即返回受管源描述。
auto try_make_managed_source(const ManagedPaths& paths, const std::filesystem::path& file_path,
                             bool require_existing_file) -> std::optional<ManagedSource> {
  // 先按照片规则判，再按录像规则判。
  // 这里是“OR 语义”：只要命中其中一个规则就算受管文件。
  if (auto photo_source = try_make_photo_source(paths, file_path, require_existing_file);
      photo_source.has_value()) {
    return photo_source;
  }

  if (auto video_source = try_make_video_source(paths, file_path, require_existing_file);
      video_source.has_value()) {
    return video_source;
  }

  return std::nullopt;
}

// 全量扫描来源目录，收集受管源文件并检测目标链接冲突。
auto collect_source_assets(
    const ManagedPaths& paths,
    const std::function<void(const InfinityNikkiInitializeMediaHardlinksProgress&)>&
        progress_callback,
    InfinityNikkiInitializeMediaHardlinksResult& result)
    -> std::expected<std::vector<ManagedSource>, std::string> {
  // 全量扫描阶段：从 gameplay_photos_dir 枚举所有文件，筛出“受管源文件”。
  // 重要：这里只负责“收集计划”，真正写硬链接在后面的 sync_hardlinks_internal。
  std::vector<ManagedSource> sources;
  // link_targets 用于检测“不同源文件映射到同一个目标链接名”的冲突；冲突时直接失败，
  // 避免在同一次同步里出现非确定性覆盖。
  std::unordered_map<std::string, std::filesystem::path> link_targets;
  std::error_code ec;

  report_progress(progress_callback, "preparing", 0, 0, 5.0, "Scanning Infinity Nikki media");

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
    auto source = try_make_managed_source(paths, file_path, true);
    if (!source.has_value()) {
      continue;
    }

    auto link_key = make_path_compare_key(source->link_path);
    if (auto it = link_targets.find(link_key); it != link_targets.end()) {
      return std::unexpected("Duplicate Infinity Nikki managed link target detected: '" +
                             Utils::String::ToUtf8(source->link_path.wstring()) + "' from '" +
                             Utils::String::ToUtf8(it->second.wstring()) + "' and '" +
                             Utils::String::ToUtf8(source->target_path.wstring()) + "'");
    }

    link_targets.emplace(link_key, source->target_path);
    sources.push_back(std::move(*source));
  }

  std::ranges::sort(sources, [](const ManagedSource& lhs, const ManagedSource& rhs) {
    // 排序保证执行顺序稳定，便于日志追踪和问题复现。
    if (lhs.kind != rhs.kind) {
      return lhs.kind < rhs.kind;
    }
    return lhs.link_path.filename().wstring() < rhs.link_path.filename().wstring();
  });

  result.source_count = static_cast<std::int32_t>(sources.size());
  return sources;
}

// 确保目录存在（不存在则递归创建）。
auto ensure_directory_exists(const std::filesystem::path& path)
    -> std::expected<void, std::string> {
  // create_directories 是幂等的：目录已存在也不会报错。
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    return std::unexpected("Failed to create directory '" + Utils::String::ToUtf8(path.wstring()) +
                           "': " + ec.message());
  }
  return {};
}

// 判断两个文件系统条目是否等价（是否指向同一实体）。
auto are_equivalent_entries(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
    -> bool {
  // 等价判定依赖系统调用；失败时按“不等价”处理更安全（返回 false）。
  std::error_code ec;
  auto equivalent = std::filesystem::equivalent(lhs, rhs, ec);
  return !ec && equivalent;
}

// 确保 link_path 是指向 target_path 的硬链接，必要时创建或替换。
auto ensure_hardlink(const std::filesystem::path& link_path,
                     const std::filesystem::path& target_path)
    -> std::expected<LinkWriteAction, std::string> {
  // 保护：禁止把链接写回自身路径（等价路径也不允许）。
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
    return std::unexpected("Failed to inspect managed link entry '" +
                           Utils::String::ToUtf8(link_path.wstring()) + "': " + ec.message());
  }

  if (link_exists && are_equivalent_entries(link_path, target_path)) {
    // 已经指向同一 inode，无需重建。
    return LinkWriteAction::kNone;
  }

  auto action = link_exists ? LinkWriteAction::kUpdated : LinkWriteAction::kCreated;
  if (link_exists) {
    // 这里用 remove_all 处理“路径上已有文件/目录/旧链接”等各种历史形态。
    std::filesystem::remove_all(link_path, ec);
    if (ec) {
      return std::unexpected("Failed to replace existing managed link entry '" +
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

// 删除受管硬链接条目；不存在时返回 false 而非错误。
auto remove_managed_link(const std::filesystem::path& link_path)
    -> std::expected<bool, std::string> {
  // 返回值语义：
  // - true：确实删掉了一个条目
  // - false：路径本来就不存在（不是错误）
  std::error_code ec;
  if (!std::filesystem::exists(link_path, ec)) {
    if (ec) {
      return std::unexpected("Failed to inspect managed link entry '" +
                             Utils::String::ToUtf8(link_path.wstring()) + "': " + ec.message());
    }
    return false;
  }

  std::filesystem::remove_all(link_path, ec);
  if (ec) {
    return std::unexpected("Failed to remove managed link entry '" +
                           Utils::String::ToUtf8(link_path.wstring()) + "': " + ec.message());
  }

  return true;
}

// 回收录像 session 目录，并与 watcher 协作避免误触发扫描事件。
auto remove_video_session_directory(Core::State::AppState& app_state,
                                    const std::filesystem::path& session_directory)
    -> std::expected<bool, std::string> {
  // 这里是“附加清理”：
  // 删除某些录像硬链接后，可能对应一个空的/无用的 session 文件夹，
  // 该函数负责把它移动到回收站（不是硬删），降低误删风险。
  std::error_code ec;
  if (!std::filesystem::exists(session_directory, ec)) {
    if (ec) {
      return std::unexpected("Failed to inspect video session directory '" +
                             Utils::String::ToUtf8(session_directory.wstring()) +
                             "': " + ec.message());
    }
    return false;
  }

  if (!std::filesystem::is_directory(session_directory, ec)) {
    if (ec) {
      return std::unexpected("Failed to inspect video session directory '" +
                             Utils::String::ToUtf8(session_directory.wstring()) +
                             "': " + ec.message());
    }
    return false;
  }

  // 删除录像 session 目录前后都要与 watcher 协作，避免把我们主动清理当成外部文件事件。
  auto begin_ignore_result = Features::Gallery::Watcher::begin_manual_move_ignore(
      app_state, session_directory, session_directory);
  bool ignore_registered = begin_ignore_result.has_value();
  if (!begin_ignore_result) {
    return std::unexpected("Failed to register watcher ignore for video session cleanup '" +
                           Utils::String::ToUtf8(session_directory.wstring()) +
                           "': " + begin_ignore_result.error());
  }

  auto complete_ignore = [&]() -> std::expected<void, std::string> {
    // 对 begin/complete 成对调用，避免 watcher ignore 状态泄漏。
    if (!ignore_registered) {
      return {};
    }

    auto complete_ignore_result = Features::Gallery::Watcher::complete_manual_move_ignore(
        app_state, session_directory, session_directory);
    if (!complete_ignore_result) {
      return std::unexpected("Failed to complete watcher ignore for video session cleanup '" +
                             Utils::String::ToUtf8(session_directory.wstring()) +
                             "': " + complete_ignore_result.error());
    }

    ignore_registered = false;
    return {};
  };

  auto recycle_result = Utils::System::move_files_to_recycle_bin({session_directory});
  auto complete_ignore_result = complete_ignore();
  if (!complete_ignore_result) {
    return std::unexpected(complete_ignore_result.error());
  }

  if (!recycle_result) {
    return std::unexpected("Failed to move video session directory to recycle bin '" +
                           Utils::String::ToUtf8(session_directory.wstring()) +
                           "': " + recycle_result.error());
  }

  Logger().info("InfinityNikki removed video session directory: {}",
                Utils::String::ToUtf8(session_directory.wstring()));
  return true;
}

// 全量同步主流程：收集受管源并逐条创建/更新硬链接。
auto sync_hardlinks_internal(
    Core::State::AppState& app_state,
    const std::function<void(const InfinityNikkiInitializeMediaHardlinksProgress&)>&
        progress_callback)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string> {
  // 全量同步主流程：
  // 1) 解析目录 -> 2) 确保投影目录存在 -> 3) 收集受管源 -> 4) 逐个创建/更新硬链接
  // 任何“流程级失败”直接返回 unexpected；单文件失败记录到 result.errors 后继续。
  auto paths_result = resolve_managed_paths(app_state);
  if (!paths_result) {
    return std::unexpected(paths_result.error());
  }
  const auto paths = paths_result.value();

  InfinityNikkiInitializeMediaHardlinksResult result;

  auto ensure_screenshot_dir_result = ensure_directory_exists(paths.screenshot_dir);
  if (!ensure_screenshot_dir_result) {
    return std::unexpected(ensure_screenshot_dir_result.error());
  }

  auto ensure_video_dir_result = ensure_directory_exists(paths.video_dir);
  if (!ensure_video_dir_result) {
    return std::unexpected(ensure_video_dir_result.error());
  }

  auto sources_result = collect_source_assets(paths, progress_callback, result);
  if (!sources_result) {
    return std::unexpected(sources_result.error());
  }
  auto sources = std::move(sources_result.value());

  // 进度规划：
  // preparing ~5%（枚举来源）
  // syncing 20%~95%（逐个写链接）
  // completed 100%
  report_progress(progress_callback, "syncing", 0, result.source_count, 20.0,
                  std::format("Found {} Infinity Nikki managed media files", result.source_count));

  for (std::size_t index = 0; index < sources.size(); ++index) {
    const auto& source = sources[index];
    report_progress(
        progress_callback, "syncing", static_cast<std::int64_t>(index), result.source_count,
        20.0 + (static_cast<double>(index) * 75.0 / std::max<std::int32_t>(result.source_count, 1)),
        std::format("Syncing {}", source.original_filename));

    auto ensure_result = ensure_hardlink(source.link_path, source.target_path);
    if (!ensure_result) {
      // 单文件失败不终止全局任务，尽量“能修多少修多少”。
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

// 增量同步主流程：消费 Gallery changes 并增量维护硬链接。
auto apply_runtime_changes(Core::State::AppState& app_state,
                           const std::vector<Features::Gallery::Types::ScanChange>& changes)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string> {
  // 增量同步主流程（由 Gallery watcher 的变化集触发）：
  // - UPSERT：确保对应硬链接存在且指向最新文件
  // - REMOVE：删除对应硬链接，录像再尝试清理 session 目录
  // 目标是把每次扫描变更快速投影到受管目录，而不是重新全量扫描。
  auto paths_result = resolve_managed_paths(app_state);
  if (!paths_result) {
    return std::unexpected(paths_result.error());
  }
  const auto paths = paths_result.value();

  InfinityNikkiInitializeMediaHardlinksResult result;
  result.source_count = static_cast<std::int32_t>(changes.size());
  Logger().info("InfinityNikki apply_runtime_changes: changes={}", changes.size());

  for (const auto& change : changes) {
    // 增量路径只处理“仍满足受管规则”的变化；其余变更统一计入 ignored。
    auto changed_path = normalize_existing_path(std::filesystem::path(change.path));
    auto source = try_make_managed_source(paths, changed_path, false);
    if (!source.has_value()) {
      if (change.action == Features::Gallery::Types::ScanChangeAction::REMOVE) {
        Logger().warn(
            "InfinityNikki ignored REMOVE change because path is not a managed media source: {}",
            Utils::String::ToUtf8(changed_path.wstring()));
      } else {
        Logger().debug(
            "InfinityNikki ignored change because path is not a managed media source: {}",
            Utils::String::ToUtf8(changed_path.wstring()));
      }
      result.ignored_count++;
      continue;
    }

    switch (change.action) {
      case Features::Gallery::Types::ScanChangeAction::UPSERT: {
        Logger().debug("InfinityNikki processing UPSERT: kind={}, source='{}', link='{}'",
                       source->kind == ManagedSourceKind::kVideo ? "video" : "photo",
                       Utils::String::ToUtf8(changed_path.wstring()),
                       Utils::String::ToUtf8(source->link_path.wstring()));
        std::error_code ec;
        if (!std::filesystem::is_regular_file(changed_path, ec) || ec) {
          if (ec) {
            add_error(result.errors, "Failed to inspect runtime source '" +
                                         Utils::String::ToUtf8(changed_path.wstring()) +
                                         "': " + ec.message());
          } else {
            result.ignored_count++;
          }
          break;
        }

        auto ensure_result = ensure_hardlink(source->link_path, changed_path);
        if (!ensure_result) {
          add_error(result.errors, ensure_result.error());
          break;
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
        break;
      }
      case Features::Gallery::Types::ScanChangeAction::REMOVE: {
        Logger().info("InfinityNikki processing REMOVE: kind={}, source='{}', link='{}'",
                      source->kind == ManagedSourceKind::kVideo ? "video" : "photo",
                      Utils::String::ToUtf8(changed_path.wstring()),
                      Utils::String::ToUtf8(source->link_path.wstring()));
        // 录像删除时除了删除投影硬链接，还会尝试回收对应 session 目录。
        auto remove_result = remove_managed_link(source->link_path);
        if (!remove_result) {
          add_error(result.errors, remove_result.error());
          break;
        }

        if (remove_result.value()) {
          result.removed_count++;
          Logger().info("InfinityNikki removed managed hardlink: {}",
                        Utils::String::ToUtf8(source->link_path.wstring()));
        } else {
          Logger().info("InfinityNikki managed hardlink already absent: {}",
                        Utils::String::ToUtf8(source->link_path.wstring()));
        }

        if (source->kind == ManagedSourceKind::kVideo && source->session_directory.has_value()) {
          auto cleanup_result =
              remove_video_session_directory(app_state, *source->session_directory);
          if (!cleanup_result) {
            add_error(result.errors, cleanup_result.error());
          } else if (!cleanup_result.value()) {
            Logger().info("InfinityNikki video session directory already absent: {}",
                          Utils::String::ToUtf8(source->session_directory->wstring()));
          }
        }
        break;
      }
      default:
        result.ignored_count++;
        break;
    }
  }

  return result;
}

// 统一消费一次 Gallery 扫描结果，收口“增量 / no-op / 全量回退”的决策。
auto apply_scan_result(Core::State::AppState& app_state,
                       const Features::Gallery::Types::ScanResult& result)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string> {
  // 最优路径：Gallery 已明确给出逐文件变化，直接走增量同步。
  if (!result.changes.empty()) {
    Logger().info("InfinityNikki apply_scan_result: mode=incremental, changes={}",
                  result.changes.size());
    return apply_runtime_changes(app_state, result.changes);
  }

  // 完全无变化时什么都不做，避免 initial scan 这类路径额外触发一次全量 sync。
  if (result.new_items == 0 && result.updated_items == 0 && result.deleted_items == 0) {
    Logger().info("InfinityNikki apply_scan_result: mode=noop");
    return InfinityNikkiInitializeMediaHardlinksResult{};
  }

  // 正常情况下不应出现“有统计变化但没有逐文件 changes”；
  // 若未来上游某条扫描链路破坏了这个不变量，这里至少留下一条可见告警。
  Logger().warn(
      "InfinityNikki MediaHardlinks fallback to full sync: scan result has counters but no "
      "changes. new={}, updated={}, deleted={}",
      result.new_items, result.updated_items, result.deleted_items);

  // 兜底：回退到全量收敛，保证受管投影目录最终一致。
  return sync(app_state);
}

// 对外全量初始化入口（带进度回调）。
auto initialize(Core::State::AppState& app_state,
                const std::function<void(const InfinityNikkiInitializeMediaHardlinksProgress&)>&
                    progress_callback)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string> {
  // initialize 是“带进度”的全量入口，通常给首次初始化任务调用。
  return sync_hardlinks_internal(app_state, progress_callback);
}

// 对外静默全量同步入口（不回调进度）。
auto sync(Core::State::AppState& app_state)
    -> std::expected<InfinityNikkiInitializeMediaHardlinksResult, std::string> {
  // sync 是“静默”全量入口，通常给后台自动修正调用。
  return sync_hardlinks_internal(app_state, nullptr);
}

// 返回应被 watcher 监听的根目录（GamePlayPhotos）。
auto resolve_watch_directory(Core::State::AppState& app_state)
    -> std::expected<std::filesystem::path, std::string> {
  // PhotoService 监听这个目录；产生 changes 后再交给 apply_runtime_changes。
  auto paths_result = resolve_managed_paths(app_state);
  if (!paths_result) {
    return std::unexpected(paths_result.error());
  }
  return paths_result->gameplay_photos_dir;
}

}  // namespace Extensions::InfinityNikki::MediaHardlinks
