module;

#include <windows.h>
#include <winioctl.h>

module Features.Gallery.Recovery.Service;

import std;
import Core.State;
import Features.Gallery.State;
import Features.Gallery.Types;
import Features.Gallery.Recovery.Types;
import Features.Gallery.Recovery.Repository;
import Features.Gallery.Ignore.Repository;
import Features.Gallery.ScanCommon;
import Utils.Logger;
import Utils.Path;
import Utils.String;

namespace Features::Gallery::Recovery::Service::Detail {

// RAII 句柄包装，避免提前 return 时泄漏 HANDLE。
struct UniqueHandle {
  HANDLE value{INVALID_HANDLE_VALUE};

  UniqueHandle() = default;
  explicit UniqueHandle(HANDLE handle) : value(handle) {}

  UniqueHandle(const UniqueHandle&) = delete;
  auto operator=(const UniqueHandle&) -> UniqueHandle& = delete;

  UniqueHandle(UniqueHandle&& other) noexcept : value(other.value) {
    other.value = INVALID_HANDLE_VALUE;
  }

  auto operator=(UniqueHandle&& other) noexcept -> UniqueHandle& {
    if (this == &other) {
      return *this;
    }
    reset();
    value = other.value;
    other.value = INVALID_HANDLE_VALUE;
    return *this;
  }

  ~UniqueHandle() { reset(); }

  auto reset(HANDLE handle = INVALID_HANDLE_VALUE) -> void {
    if (value != INVALID_HANDLE_VALUE && value != nullptr) {
      CloseHandle(value);
    }
    value = handle;
  }

  [[nodiscard]] auto valid() const -> bool {
    return value != INVALID_HANDLE_VALUE && value != nullptr;
  }
};

struct JournalSnapshot {
  // available=false 不代表出错，只是表示当前 root 不支持 USN 恢复（如非 NTFS、网络盘等）。
  bool available = false;
  std::string reason;
  std::wstring volume_root;
  std::wstring volume_device_path;
  std::string volume_identity;
  std::int64_t journal_id = 0;
  std::int64_t next_usn = 0;
};

struct UsnRecordView {
  // 轻量视图，只保留 recovery 实际需要的字段，屏蔽 Win32 结构体细节。
  std::int64_t file_reference_number = 0;
  std::int64_t parent_file_reference_number = 0;
  std::int64_t usn = 0;
  DWORD reason = 0;
  DWORD file_attributes = 0;
  std::filesystem::path file_name;
};

auto normalize_existing_path(const std::filesystem::path& path) -> std::filesystem::path {
  // 跨多数据源（DB、USN、文件系统）比较路径时，统一解析为标准形态，消除大小写 / 分隔符差异。
  std::error_code ec;
  if (std::filesystem::exists(path, ec) && !ec) {
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
      return normalized;
    }
  }
  return path.lexically_normal();
}

auto make_path_compare_key(const std::filesystem::path& path) -> std::string {
  // NTFS 路径大小写不敏感，统一转小写 generic path 用于比较。
  auto normalized = normalize_existing_path(path).generic_wstring();
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return Utils::String::ToUtf8(normalized);
}

auto is_path_under_root(const std::filesystem::path& path, const std::string& root_key) -> bool {
  // USN 是按卷读取的，记录可能属于同卷上其他目录。
  // 这里根据预计算的 root_key 过滤，只保留当前监视根目录下的路径。
  auto path_key = make_path_compare_key(path);
  if (path_key == root_key) {
    return true;
  }
  if (!path_key.starts_with(root_key)) {
    return false;
  }
  return path_key.size() > root_key.size() && path_key[root_key.size()] == '/';
}

auto strip_extended_path_prefix(std::wstring value) -> std::wstring {
  // GetFinalPathNameByHandleW 返回的路径常带 \\?\ 或 \\?\UNC\ 前缀，去掉以保持一致。
  constexpr std::wstring_view extended_prefix = L"\\\\?\\";
  constexpr std::wstring_view unc_prefix = L"UNC\\";
  if (value.starts_with(extended_prefix)) {
    value.erase(0, extended_prefix.size());
    if (value.starts_with(unc_prefix)) {
      value.erase(0, unc_prefix.size());
      value.insert(0, L"\\\\");
    }
  }
  return value;
}

auto get_volume_root_for_path(const std::filesystem::path& path)
    -> std::expected<std::wstring, std::string> {
  auto path_w = normalize_existing_path(path).wstring();
  std::wstring buffer(MAX_PATH, L'\0');
  if (!GetVolumePathNameW(path_w.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()))) {
    return std::unexpected("GetVolumePathNameW failed: " + std::to_string(GetLastError()));
  }
  buffer.resize(std::wcslen(buffer.c_str()));
  return buffer;
}

auto make_volume_device_path(const std::wstring& volume_root)
    -> std::expected<std::wstring, std::string> {
  if (volume_root.size() >= 2 && volume_root[1] == L':') {
    return std::wstring{L"\\\\.\\"} + volume_root.substr(0, 2);
  }
  return std::unexpected("Only drive-letter NTFS volumes are supported for USN recovery");
}

auto open_volume_handle(const std::wstring& volume_device_path)
    -> std::expected<UniqueHandle, std::string> {
  HANDLE handle = CreateFileW(volume_device_path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return std::unexpected("Failed to open volume handle: " + std::to_string(GetLastError()));
  }
  return UniqueHandle{handle};
}

// 查询指定 root 所在卷的 USN Journal 快照。root_path 必须已归一化。
auto query_journal_snapshot(const std::filesystem::path& root_path)
    -> std::expected<JournalSnapshot, std::string> {
  JournalSnapshot snapshot;

  auto volume_root_result = get_volume_root_for_path(root_path);
  if (!volume_root_result) {
    return std::unexpected(volume_root_result.error());
  }
  snapshot.volume_root = volume_root_result.value();

  // v1 只支持本地 NTFS 卷，网络盘 / 非 NTFS 直接视为不支持 USN。
  if (GetDriveTypeW(snapshot.volume_root.c_str()) == DRIVE_REMOTE) {
    snapshot.reason = "remote volume does not expose a local USN journal";
    return snapshot;
  }

  wchar_t file_system_name[MAX_PATH]{};
  DWORD volume_serial_number = 0;
  if (!GetVolumeInformationW(snapshot.volume_root.c_str(), nullptr, 0, &volume_serial_number,
                             nullptr, nullptr, file_system_name, MAX_PATH)) {
    return std::unexpected("GetVolumeInformationW failed: " + std::to_string(GetLastError()));
  }

  auto file_system =
      Utils::String::ToLowerAscii(Utils::String::ToUtf8(std::wstring(file_system_name)));
  if (file_system != "ntfs") {
    snapshot.reason = "only NTFS volumes use USN startup recovery";
    return snapshot;
  }

  // 查询当前 Journal 的 ID 和 next_usn，启动恢复将用它们与 DB 中保存的检查点做比对。
  auto volume_device_path_result = make_volume_device_path(snapshot.volume_root);
  if (!volume_device_path_result) {
    snapshot.reason = volume_device_path_result.error();
    return snapshot;
  }
  snapshot.volume_device_path = volume_device_path_result.value();

  auto volume_handle_result = open_volume_handle(snapshot.volume_device_path);
  if (!volume_handle_result) {
    snapshot.reason = volume_handle_result.error();
    return snapshot;
  }

  USN_JOURNAL_DATA_V1 journal_data{};
  DWORD bytes_returned = 0;
  if (!DeviceIoControl(volume_handle_result->value, FSCTL_QUERY_USN_JOURNAL, nullptr, 0,
                       &journal_data, sizeof(journal_data), &bytes_returned, nullptr)) {
    snapshot.reason = "FSCTL_QUERY_USN_JOURNAL failed: " + std::to_string(GetLastError());
    return snapshot;
  }

  snapshot.available = true;
  snapshot.volume_identity = std::format("ntfs:{:08X}", volume_serial_number);
  snapshot.journal_id = static_cast<std::int64_t>(journal_data.UsnJournalID);
  snapshot.next_usn = static_cast<std::int64_t>(journal_data.NextUsn);
  return snapshot;
}

// 加载指定 root 的全部忽略规则（global + root-specific）。root_path 必须已归一化。
auto get_root_rules(Core::State::AppState& app_state, const std::filesystem::path& root_path)
    -> std::expected<std::vector<Features::Gallery::Types::IgnoreRule>, std::string> {
  // fingerprint 需要覆盖所有影响扫描结果的规则，因此合并 global 和 root-specific 规则。
  auto global_rules_result = Features::Gallery::Ignore::Repository::get_global_rules(app_state);
  if (!global_rules_result) {
    return std::unexpected(global_rules_result.error());
  }

  auto root_rules_result = Features::Gallery::Ignore::Repository::get_rules_by_directory_path(
      app_state, root_path.string());
  if (!root_rules_result) {
    return std::unexpected(root_rules_result.error());
  }

  auto rules = std::move(global_rules_result.value());
  auto root_rules = std::move(root_rules_result.value());
  rules.insert(rules.end(), std::make_move_iterator(root_rules.begin()),
               std::make_move_iterator(root_rules.end()));
  return rules;
}

auto make_rule_fingerprint(Core::State::AppState& app_state, const std::filesystem::path& root_path,
                           const Features::Gallery::Types::ScanOptions& scan_options)
    -> std::expected<std::string, std::string> {
  // v1 不需要加密哈希，稳定排序后拼为文本即可。目标是检测扫描规则是否变化。
  std::vector<std::string> lines;

  auto supported_extensions = scan_options.supported_extensions.has_value()
                                  ? *scan_options.supported_extensions
                                  : Features::Gallery::ScanCommon::default_supported_extensions();
  for (auto& extension : supported_extensions) {
    extension = Utils::String::ToLowerAscii(extension);
  }
  std::ranges::sort(supported_extensions);
  for (const auto& extension : supported_extensions) {
    lines.push_back("ext:" + extension);
  }

  auto rules_result = get_root_rules(app_state, root_path);
  if (!rules_result) {
    return std::unexpected("Failed to build rule fingerprint: " + rules_result.error());
  }

  for (const auto& rule : rules_result.value()) {
    auto scope = rule.folder_id.has_value() ? "root" : "global";
    lines.push_back(std::format("rule:{}|{}|{}|{}", scope, rule.pattern_type, rule.rule_type,
                                rule.rule_pattern));
  }

  lines.push_back("scan_semantics:v1");
  std::ranges::sort(lines);

  std::string fingerprint;
  for (const auto& line : lines) {
    fingerprint += line;
    fingerprint.push_back('\n');
  }
  return fingerprint;
}

auto make_file_id_descriptor(std::int64_t file_reference_number) -> FILE_ID_DESCRIPTOR {
  FILE_ID_DESCRIPTOR descriptor{};
  descriptor.dwSize = sizeof(descriptor);
  descriptor.Type = FileIdType;
  descriptor.FileId.QuadPart = static_cast<LONGLONG>(file_reference_number);
  return descriptor;
}

auto resolve_path_by_file_reference(
    HANDLE volume_handle, std::int64_t file_reference_number,
    std::unordered_map<std::int64_t, std::optional<std::filesystem::path>>& cache)
    -> std::expected<std::optional<std::filesystem::path>, std::string> {
  // USN 记录通常只给 file reference number (FRN)，
  // 需要通过 OpenFileById 将 FRN 解析为绝对路径，才能供 gallery 增量链路使用。
  if (auto it = cache.find(file_reference_number); it != cache.end()) {
    return it->second;
  }

  auto descriptor = make_file_id_descriptor(file_reference_number);
  HANDLE handle = OpenFileById(volume_handle, &descriptor, FILE_READ_ATTRIBUTES,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               FILE_FLAG_BACKUP_SEMANTICS);
  // 文件已删除或无权限访问时 OpenFileById 会失败。
  // USN 按卷读取，常遇到回收站、System Volume Information 等受保护路径，
  // 这些通常不是 gallery 关心的资源，与 FILE_NOT_FOUND 同等处理即可。
  if (handle == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
        error == ERROR_INVALID_PARAMETER || error == ERROR_ACCESS_DENIED) {
      cache[file_reference_number] = std::nullopt;
      return std::optional<std::filesystem::path>{};
    }
    return std::unexpected("OpenFileById failed: " + std::to_string(error));
  }

  UniqueHandle scoped_handle{handle};

  std::wstring buffer(MAX_PATH, L'\0');
  DWORD length = GetFinalPathNameByHandleW(scoped_handle.value, buffer.data(),
                                           static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
  if (length == 0) {
    return std::unexpected("GetFinalPathNameByHandleW failed: " + std::to_string(GetLastError()));
  }
  if (length >= buffer.size()) {
    buffer.resize(length + 1, L'\0');
    length = GetFinalPathNameByHandleW(scoped_handle.value, buffer.data(),
                                       static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
    if (length == 0) {
      return std::unexpected("GetFinalPathNameByHandleW failed: " + std::to_string(GetLastError()));
    }
  }

  buffer.resize(length);
  auto resolved_path =
      normalize_existing_path(std::filesystem::path(strip_extended_path_prefix(buffer)));
  cache[file_reference_number] = resolved_path;
  return resolved_path;
}

auto parse_usn_record(const std::byte* record_bytes) -> std::expected<UsnRecordView, std::string> {
  // 先只处理 V2 记录，其他版本明确拒绝以避免误解析。
  auto* record_header = reinterpret_cast<const USN_RECORD_COMMON_HEADER*>(record_bytes);
  if (record_header->MajorVersion != 2) {
    return std::unexpected("Only USN_RECORD_V2 is supported in v1 startup recovery");
  }

  auto* record = reinterpret_cast<const USN_RECORD_V2*>(record_bytes);
  auto file_name_chars = record->FileNameLength / sizeof(WCHAR);
  auto file_name_ptr = reinterpret_cast<const wchar_t*>(reinterpret_cast<const std::byte*>(record) +
                                                        record->FileNameOffset);

  return UsnRecordView{
      .file_reference_number = static_cast<std::int64_t>(record->FileReferenceNumber),
      .parent_file_reference_number = static_cast<std::int64_t>(record->ParentFileReferenceNumber),
      .usn = static_cast<std::int64_t>(record->Usn),
      .reason = record->Reason,
      .file_attributes = record->FileAttributes,
      .file_name = std::filesystem::path(std::wstring(file_name_ptr, file_name_chars)),
  };
}

auto is_directory_record(const UsnRecordView& record) -> bool {
  return (record.file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

auto resolve_parent_based_path(
    HANDLE volume_handle, const UsnRecordView& record,
    std::unordered_map<std::int64_t, std::optional<std::filesystem::path>>& path_cache)
    -> std::expected<std::optional<std::filesystem::path>, std::string> {
  // 删除 / rename-old-name 时目标文件可能已不存在，
  // 只能通过父目录路径 + 文件名来重建旧路径。
  auto parent_path_result = resolve_path_by_file_reference(
      volume_handle, record.parent_file_reference_number, path_cache);
  if (!parent_path_result) {
    return std::unexpected(parent_path_result.error());
  }
  if (!parent_path_result->has_value()) {
    return std::optional<std::filesystem::path>{};
  }

  return parent_path_result->value() / record.file_name;
}

auto add_or_replace_change(
    std::unordered_map<std::string, Features::Gallery::Types::ScanChangeAction>& changes_by_path,
    const std::filesystem::path& path, Features::Gallery::Types::ScanChangeAction action) -> void {
  // 同一路径离线期间可能被多次修改，只保留最终需要达到的状态。
  changes_by_path[normalize_existing_path(path).string()] = action;
}

auto collect_usn_changes(const std::filesystem::path& root_path, std::int64_t start_usn,
                         const JournalSnapshot& snapshot)
    -> std::expected<std::vector<Features::Gallery::Types::ScanChange>, std::string> {
  // 核心流程：从上次检查点 start_usn 开始读取 Journal，
  // 筛出当前 root 下的文件变更，翻译成 ScanChange 列表。
  if (start_usn >= snapshot.next_usn) {
    return std::vector<Features::Gallery::Types::ScanChange>{};
  }

  auto volume_handle_result = open_volume_handle(snapshot.volume_device_path);
  if (!volume_handle_result) {
    return std::unexpected(volume_handle_result.error());
  }

  auto normalized_root = normalize_existing_path(root_path);
  auto root_key = make_path_compare_key(normalized_root);
  std::unordered_map<std::int64_t, std::optional<std::filesystem::path>> path_cache;
  std::unordered_map<std::string, Features::Gallery::Types::ScanChangeAction> changes_by_path;

  constexpr DWORD kBufferSize = 64 * 1024;
  std::vector<std::byte> buffer(kBufferSize);

  // READ_USN_JOURNAL_DATA_V1 指定读取起点、目标 Journal、及可接受的记录版本。
  READ_USN_JOURNAL_DATA_V1 read_data{};
  read_data.StartUsn = static_cast<USN>(start_usn);
  read_data.ReasonMask = 0xFFFFFFFF;
  read_data.ReturnOnlyOnClose = FALSE;
  read_data.Timeout = 0;
  read_data.BytesToWaitFor = 0;
  read_data.UsnJournalID = static_cast<DWORDLONG>(snapshot.journal_id);
  read_data.MinMajorVersion = 2;
  read_data.MaxMajorVersion = 2;

  while (static_cast<std::int64_t>(read_data.StartUsn) < snapshot.next_usn) {
    DWORD bytes_returned = 0;
    if (!DeviceIoControl(volume_handle_result->value, FSCTL_READ_USN_JOURNAL, &read_data,
                         sizeof(read_data), buffer.data(), static_cast<DWORD>(buffer.size()),
                         &bytes_returned, nullptr)) {
      auto error = GetLastError();
      if (error == ERROR_HANDLE_EOF) {
        break;
      }
      return std::unexpected("FSCTL_READ_USN_JOURNAL failed: " + std::to_string(error));
    }

    if (bytes_returned < sizeof(USN)) {
      break;
    }

    auto next_start_usn = *reinterpret_cast<const USN*>(buffer.data());
    std::size_t offset = sizeof(USN);

    while (offset + sizeof(USN_RECORD_COMMON_HEADER) <= bytes_returned) {
      auto* record_base = buffer.data() + offset;
      auto* common_header = reinterpret_cast<const USN_RECORD_COMMON_HEADER*>(record_base);
      if (common_header->RecordLength == 0 ||
          offset + common_header->RecordLength > bytes_returned) {
        break;
      }

      auto record_result = parse_usn_record(record_base);
      if (!record_result) {
        return std::unexpected(record_result.error());
      }

      const auto& record = record_result.value();
      if (record.usn > snapshot.next_usn) {
        break;
      }

      std::optional<std::filesystem::path> candidate_path;
      if ((record.reason & (USN_REASON_FILE_DELETE | USN_REASON_RENAME_OLD_NAME)) != 0) {
        auto parent_based_result =
            resolve_parent_based_path(volume_handle_result->value, record, path_cache);
        if (!parent_based_result) {
          return std::unexpected(parent_based_result.error());
        }
        candidate_path = parent_based_result.value();
      } else {
        auto resolved_path_result = resolve_path_by_file_reference(
            volume_handle_result->value, record.file_reference_number, path_cache);
        if (!resolved_path_result) {
          return std::unexpected(resolved_path_result.error());
        }
        candidate_path = resolved_path_result.value();
      }

      if (!candidate_path.has_value() || !is_path_under_root(*candidate_path, root_key)) {
        offset += common_header->RecordLength;
        continue;
      }

      // v1 对目录结构变化采取最保守策略：直接回退 full scan。
      // 目录重命名 / 删除影响大量子路径，增量恢复难以正确处理。
      if (is_directory_record(record)) {
        return std::unexpected("Directory structural changes require a full rescan");
      }

      if ((record.reason & (USN_REASON_FILE_DELETE | USN_REASON_RENAME_OLD_NAME)) != 0) {
        add_or_replace_change(changes_by_path, *candidate_path,
                              Features::Gallery::Types::ScanChangeAction::REMOVE);
      }

      if ((record.reason & (USN_REASON_FILE_CREATE | USN_REASON_DATA_OVERWRITE |
                            USN_REASON_DATA_EXTEND | USN_REASON_DATA_TRUNCATION |
                            USN_REASON_BASIC_INFO_CHANGE | USN_REASON_RENAME_NEW_NAME)) != 0) {
        add_or_replace_change(changes_by_path, *candidate_path,
                              Features::Gallery::Types::ScanChangeAction::UPSERT);
      }

      offset += common_header->RecordLength;
    }

    if (next_start_usn <= read_data.StartUsn) {
      break;
    }
    read_data.StartUsn = next_start_usn;
  }

  std::vector<Features::Gallery::Types::ScanChange> changes;
  changes.reserve(changes_by_path.size());
  for (const auto& [path, action] : changes_by_path) {
    changes.push_back(Features::Gallery::Types::ScanChange{.path = path, .action = action});
  }
  std::ranges::sort(changes, [](const auto& lhs, const auto& rhs) { return lhs.path < rhs.path; });
  return changes;
}

}  // namespace Features::Gallery::Recovery::Service::Detail

namespace Features::Gallery::Recovery::Service {

auto prepare_startup_recovery(Core::State::AppState& app_state,
                              const std::filesystem::path& root_path,
                              const Features::Gallery::Types::ScanOptions& scan_options)
    -> std::expected<Types::StartupRecoveryPlan, std::string> {
  Types::StartupRecoveryPlan plan;

  // 返回启动恢复决策：当前 root 应走 USN 增量还是 FullScan？
  // 只做决策，不启动 watcher。

  auto normalized_root_result = Utils::Path::NormalizePath(root_path);
  if (!normalized_root_result) {
    return std::unexpected("Failed to normalize root path: " + normalized_root_result.error());
  }
  plan.root_path = normalized_root_result->string();

  auto journal_snapshot_result = Detail::query_journal_snapshot(*normalized_root_result);
  if (!journal_snapshot_result) {
    return std::unexpected("Failed to inspect journal capability: " +
                           journal_snapshot_result.error());
  }

  const auto& snapshot = journal_snapshot_result.value();

  // 无论最终走 USN 还是 FullScan，卷快照信息都需要填入 plan，
  // 便于上层在恢复完成后直接持久化。
  plan.volume_identity = snapshot.volume_identity;
  plan.journal_id = snapshot.journal_id;
  plan.checkpoint_usn = snapshot.next_usn;

  if (!snapshot.available) {
    // 不支持 USN 不是错误，只是正常的 FullScan 回退路径。
    plan.mode = Types::StartupRecoveryMode::FullScan;
    plan.reason =
        snapshot.reason.empty() ? "journal is not available for this root" : snapshot.reason;
    return plan;
  }

  auto fingerprint_result =
      Detail::make_rule_fingerprint(app_state, *normalized_root_result, scan_options);
  if (!fingerprint_result) {
    return std::unexpected(fingerprint_result.error());
  }
  plan.rule_fingerprint = *fingerprint_result;

  auto stored_state_result =
      Repository::get_state_by_root_path(app_state, normalized_root_result->string());
  if (!stored_state_result) {
    return std::unexpected(stored_state_result.error());
  }

  if (!stored_state_result->has_value()) {
    // 没有历史检查点，可能是首次运行或旧状态已丢失，只能全量扫描。
    plan.mode = Types::StartupRecoveryMode::FullScan;
    plan.reason = "no persisted recovery checkpoint";
    return plan;
  }

  const auto& stored_state = stored_state_result->value();
  if (stored_state.volume_identity != snapshot.volume_identity) {
    plan.mode = Types::StartupRecoveryMode::FullScan;
    plan.reason = "volume identity changed";
    return plan;
  }

  if (!stored_state.journal_id.has_value() || *stored_state.journal_id != snapshot.journal_id) {
    plan.mode = Types::StartupRecoveryMode::FullScan;
    plan.reason = "journal identity changed";
    return plan;
  }

  if (stored_state.rule_fingerprint != *fingerprint_result) {
    plan.mode = Types::StartupRecoveryMode::FullScan;
    plan.reason = "scan rules changed";
    return plan;
  }

  if (!stored_state.checkpoint_usn.has_value()) {
    plan.mode = Types::StartupRecoveryMode::FullScan;
    plan.reason = "checkpoint is missing";
    return plan;
  }

  auto changes_result =
      Detail::collect_usn_changes(*normalized_root_result, *stored_state.checkpoint_usn, snapshot);
  if (!changes_result) {
    // 离线追账出现不确定情况，宁可回退全量扫描也不猜测增量。
    plan.mode = Types::StartupRecoveryMode::FullScan;
    plan.reason = "USN recovery fallback: " + changes_result.error();
    return plan;
  }

  plan.mode = Types::StartupRecoveryMode::UsnJournal;
  plan.reason = "USN recovery is available";
  plan.changes = std::move(changes_result.value());
  return plan;
}

auto persist_recovery_checkpoint(Core::State::AppState& app_state,
                                 const std::filesystem::path& root_path,
                                 const Features::Gallery::Types::ScanOptions& scan_options,
                                 std::optional<std::int64_t> checkpoint_usn)
    -> std::expected<void, std::string> {
  // 正常退出时保存检查点。需查询当前 journal 快照和规则指纹。
  // 启动恢复路径应使用轻量的 persist_recovery_state。
  auto normalized_root_result = Utils::Path::NormalizePath(root_path);
  if (!normalized_root_result) {
    return std::unexpected("Failed to normalize root path: " + normalized_root_result.error());
  }

  auto journal_snapshot_result = Detail::query_journal_snapshot(*normalized_root_result);
  if (!journal_snapshot_result) {
    return std::unexpected(journal_snapshot_result.error());
  }

  const auto& snapshot = journal_snapshot_result.value();
  if (!snapshot.available) {
    return {};
  }

  auto fingerprint_result =
      Detail::make_rule_fingerprint(app_state, *normalized_root_result, scan_options);
  if (!fingerprint_result) {
    return std::unexpected(fingerprint_result.error());
  }

  Types::WatchRootRecoveryState state{
      .root_path = normalized_root_result->string(),
      .volume_identity = snapshot.volume_identity,
      .journal_id = snapshot.journal_id,
      .checkpoint_usn = checkpoint_usn.has_value() ? checkpoint_usn
                                                   : std::optional<std::int64_t>{snapshot.next_usn},
      .rule_fingerprint = *fingerprint_result,
  };

  return Repository::upsert_state(app_state, state);
}

auto persist_recovery_state(Core::State::AppState& app_state,
                            const Types::WatchRootRecoveryState& state)
    -> std::expected<void, std::string> {
  return Repository::upsert_state(app_state, state);
}

auto persist_registered_root_checkpoints(Core::State::AppState& app_state) -> void {
  if (!app_state.gallery) {
    return;
  }

  // 先复制 root 列表再逐个 persist，避免在持久化期间长时间占用 watcher 全局锁。

  std::vector<std::pair<std::filesystem::path, Features::Gallery::Types::ScanOptions>> roots;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->folder_watchers_mutex);
    roots.reserve(app_state.gallery->folder_watchers.size());
    for (const auto& [_, watcher] : app_state.gallery->folder_watchers) {
      std::lock_guard<std::mutex> pending_lock(watcher->pending_mutex);
      roots.emplace_back(watcher->root_path, watcher->scan_options);
    }
  }

  for (const auto& [root_path, scan_options] : roots) {
    auto persist_result = persist_recovery_checkpoint(app_state, root_path, scan_options);
    if (!persist_result) {
      Logger().warn("Failed to persist gallery recovery checkpoint for '{}': {}",
                    root_path.string(), persist_result.error());
    }
  }
}

}  // namespace Features::Gallery::Recovery::Service
