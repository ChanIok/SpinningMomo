#include "features/gallery/importer/importer.hpp"

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/asset/repository.hpp"
#include "features/gallery/folder/repository.hpp"
#include "features/gallery/scanner/common.hpp"
#include "features/gallery/scanner/scanner.hpp"
#include "features/gallery/types.hpp"
#include "features/gallery/watcher/watcher.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"

namespace features::gallery::importer {
namespace {

std::atomic<std::uint64_t> import_temp_file_sequence = 0;

// 在目标目录内分配不可见的临时路径，避免 watcher 提前识别未复制完整的媒体。
auto make_import_temp_path(const std::filesystem::path& target_folder)
    -> std::expected<std::filesystem::path, std::string> {
  // 时钟值配合进程内序列，在并发批次间生成稳定不重复的候选名。
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto sequence = import_temp_file_sequence.fetch_add(1, std::memory_order_relaxed);
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    auto candidate = target_folder / std::format(".spinningmomo-import-{}-{}.tmp", ticks, sequence);

    std::error_code exists_error;
    if (!std::filesystem::exists(candidate, exists_error)) {
      if (exists_error) {
        return std::unexpected("Failed to inspect import temporary path: " +
                               exists_error.message());
      }
      return candidate;
    }
  }
  return std::unexpected("Failed to allocate a unique import temporary path");
}

// 为导入文件生成不覆盖目标，并在重名时追加递增序号。
auto make_unique_import_destination(const std::filesystem::path& target_folder,
                                    const std::filesystem::path& requested_name,
                                    std::uint64_t suffix)
    -> std::expected<std::filesystem::path, std::string> {
  // 只保留源文件名，拒绝空名称和目录跳转片段。
  auto filename = requested_name.filename();
  if (filename.empty() || filename == L"." || filename == L"..") {
    return std::unexpected("Import file name is invalid");
  }

  auto candidate = target_folder / filename;
  if (suffix > 0) {
    candidate =
        target_folder / std::filesystem::path(std::format(L"{} ({}){}", filename.stem().wstring(),
                                                          suffix, filename.extension().wstring()));
  }

  // 规范化后再次校验边界，确保组合出的名称没有逃离目标目录。
  auto normalized_result = utils::path::NormalizePath(candidate);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize import destination: " + normalized_result.error());
  }
  if (!utils::path::IsPathWithinBase(normalized_result.value(), target_folder)) {
    return std::unexpected("Import destination escaped the target folder");
  }
  return normalized_result.value();
}

// 将完整临时文件原子提交到首个空闲名称，并在最终路径出现前屏蔽 watcher。
auto commit_import_temp_file(core::AppState& app_state, const std::filesystem::path& temporary_path,
                             const std::filesystem::path& target_folder,
                             const std::filesystem::path& requested_name)
    -> std::expected<std::filesystem::path, std::string> {
  // 每次提交都重新检查目标，兼容同批次和外部程序并发创建同名文件。
  for (std::uint64_t suffix = 0; suffix < 10'000; ++suffix) {
    auto destination_result = make_unique_import_destination(target_folder, requested_name, suffix);
    if (!destination_result) {
      return std::unexpected(destination_result.error());
    }
    const auto& destination = destination_result.value();

    std::error_code exists_error;
    if (std::filesystem::exists(destination, exists_error)) {
      continue;
    }
    if (exists_error) {
      return std::unexpected("Failed to inspect import destination: " + exists_error.message());
    }

    // 最终媒体路径出现前登记屏蔽，主动入库完成前不让 watcher 重复消费。
    auto begin_ignore_result =
        watcher::begin_manual_file_system_ignore(app_state, destination, destination);
    if (!begin_ignore_result) {
      return std::unexpected("Failed to register watcher ignore for import destination '" +
                             destination.string() + "': " + begin_ignore_result.error());
    }

    // 临时文件与目标位于同一目录，rename 只在文件完整后一次性暴露最终路径。
    std::error_code rename_error;
    std::filesystem::rename(temporary_path, destination, rename_error);
    if (!rename_error) {
      return destination;
    }

    // 提交失败时立即解除本候选路径的 in-flight 状态，再决定是否尝试下一个名称。
    auto complete_result =
        watcher::complete_manual_file_system_ignore(app_state, destination, destination);
    if (!complete_result) {
      Logger().warn("Failed to complete watcher ignore for import destination '{}': {}",
                    destination.string(), complete_result.error());
    }

    std::error_code collision_error;
    if (std::filesystem::exists(destination, collision_error) && !collision_error) {
      continue;
    }
    return std::unexpected("Failed to commit imported file: " + rename_error.message());
  }
  return std::unexpected("Too many import file name collisions");
}

// 清理本次尚未提交的临时文件，不影响已经成功导入的目标。
auto cleanup_import_temp_file(const std::filesystem::path& temporary_path) -> void {
  if (temporary_path.empty()) {
    return;
  }
  std::error_code cleanup_error;
  std::filesystem::remove(temporary_path, cleanup_error);
  if (cleanup_error) {
    Logger().warn("Failed to clean import temporary file '{}': {}", temporary_path.string(),
                  cleanup_error.message());
  }
}

}  // namespace

// 将一批外部媒体复制到已索引文件夹：校验 → 临时复制 → 原子提交 → 同步入库。
auto import_files_to_folder(core::AppState& app_state, std::int64_t folder_id,
                            const std::vector<std::filesystem::path>& source_paths)
    -> std::expected<OperationResult, std::string> {
  // 目标只接受数据库中的有效文件夹 ID，调用方不能直接指定任意输出路径。
  if (folder_id <= 0) {
    return OperationResult{
        .success = false,
        .message = "Invalid target folder",
        .affected_count = 0,
    };
  }

  // 从图库索引解析并规范化真实目标目录。
  auto folder_result = folder::repository::get_folder_by_id(app_state, folder_id);
  if (!folder_result) {
    return std::unexpected("Failed to query import target folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return OperationResult{
        .success = false,
        .message = "Target folder not found",
        .affected_count = 0,
    };
  }

  auto target_result =
      utils::path::NormalizePath(std::filesystem::path(folder_result->value().path));
  if (!target_result) {
    return std::unexpected("Failed to normalize import target folder: " + target_result.error());
  }
  const auto target_folder = target_result.value();

  std::error_code target_error;
  if (!std::filesystem::is_directory(target_folder, target_error)) {
    return OperationResult{
        .success = false,
        .message = target_error ? "Failed to access target folder: " + target_error.message()
                                : "Target folder does not exist",
        .affected_count = 0,
    };
  }

  // 批次内逐项容错，最后统一汇总成功、失败、缺失和跳过数量。
  std::vector<ScanChange> indexed_changes;
  std::int64_t not_found_count = 0;
  std::int64_t unchanged_count = 0;
  std::int64_t indexed_count = 0;
  std::vector<std::string> errors;

  for (const auto& source : source_paths) {
    // 每个源路径都重新规范化并检查磁盘状态，不信任桥接层传入的字符串。
    auto normalized_source_result = utils::path::NormalizePath(source);
    if (!normalized_source_result) {
      errors.push_back("Failed to normalize import source '" + source.string() +
                       "': " + normalized_source_result.error());
      continue;
    }
    const auto normalized_source = normalized_source_result.value();

    std::error_code source_error;
    const bool source_exists = std::filesystem::exists(normalized_source, source_error);
    if (source_error) {
      errors.push_back("Failed to access import source '" + normalized_source.string() +
                       "': " + source_error.message());
      continue;
    }
    if (!source_exists) {
      not_found_count++;
      continue;
    }

    const bool is_regular_file = std::filesystem::is_regular_file(normalized_source, source_error);
    if (source_error) {
      errors.push_back("Failed to inspect import source '" + normalized_source.string() +
                       "': " + source_error.message());
      continue;
    }
    if (!is_regular_file) {
      unchanged_count++;
      continue;
    }

    // 文件夹和非媒体文件不属于当前导入范围，计为跳过而不是失败。
    const auto& supported_extensions = scanner::common::default_supported_extensions();
    if (!scanner::common::is_supported_file(normalized_source, supported_extensions)) {
      unchanged_count++;
      continue;
    }

    // 源文件已经位于目标目录时直接跳过，避免一次拖放产生无意副本。
    std::error_code equivalent_error;
    const auto direct_destination = target_folder / normalized_source.filename();
    if (std::filesystem::exists(direct_destination, equivalent_error) && !equivalent_error &&
        std::filesystem::equivalent(normalized_source, direct_destination, equivalent_error) &&
        !equivalent_error) {
      unchanged_count++;
      continue;
    }

    // 先复制到无媒体扩展名的临时文件，复制中途不会进入图库索引。
    auto temporary_result = make_import_temp_path(target_folder);
    if (!temporary_result) {
      errors.push_back(temporary_result.error());
      continue;
    }
    auto temporary_path = temporary_result.value();

    std::error_code copy_error;
    std::filesystem::copy_file(normalized_source, temporary_path,
                               std::filesystem::copy_options::none, copy_error);
    if (copy_error) {
      cleanup_import_temp_file(temporary_path);
      errors.push_back("Failed to copy import source '" + normalized_source.string() +
                       "': " + copy_error.message());
      continue;
    }

    // 文件完整后再提交最终名称；重名时由提交函数选择下一个可用名称。
    auto commit_result = commit_import_temp_file(app_state, temporary_path, target_folder,
                                                 normalized_source.filename());
    if (!commit_result) {
      cleanup_import_temp_file(temporary_path);
      errors.push_back(commit_result.error());
      continue;
    }
    const auto created_path = commit_result.value();

    // 主动文件操作同步入库，并收集真实 UPSERT 供扩展消费者处理。
    auto index_result = scanner::upsert_created_file(app_state, folder_id, created_path);
    if (!index_result) {
      errors.push_back("Failed to index imported file '" + created_path.string() +
                       "': " + index_result.error());
    } else {
      auto asset_result = asset::repository::get_asset_by_path(app_state, created_path.string());
      if (!asset_result) {
        errors.push_back("Failed to verify imported file index '" + created_path.string() +
                         "': " + asset_result.error());
      } else if (!asset_result->has_value()) {
        unchanged_count++;
      } else {
        indexed_count++;
        indexed_changes.insert(indexed_changes.end(),
                               std::make_move_iterator(index_result->changes.begin()),
                               std::make_move_iterator(index_result->changes.end()));
      }
    }

    // 本文件的磁盘和索引操作已经结束，立即退出 in-flight，延迟通知交给 grace 吸收。
    auto complete_result =
        watcher::complete_manual_file_system_ignore(app_state, created_path, created_path);
    if (!complete_result) {
      Logger().warn("Failed to complete watcher ignore for imported file '{}': {}",
                    created_path.string(), complete_result.error());
    }
  }

  // 整批文件落库后再统一分发变化，避免扩展观察到未完成的批次中间态。
  if (!indexed_changes.empty()) {
    auto dispatch_result = watcher::dispatch_manual_scan_changes(app_state, indexed_changes);
    if (!dispatch_result) {
      errors.push_back("Failed to dispatch imported file changes: " + dispatch_result.error());
    }
  }

  // OperationResult 保留完整批次统计，前端据此选择成功、部分成功或失败提示。
  const auto affected_count = indexed_count;
  const auto failed_count = static_cast<std::int64_t>(errors.size());
  OperationResult result{
      .success = affected_count > 0 && errors.empty(),
      .message = "",
      .affected_count = affected_count,
      .failed_count = failed_count,
      .not_found_count = not_found_count,
      .unchanged_count = unchanged_count,
  };

  if (result.success) {
    result.message = std::format("Imported {} file(s)", affected_count);
  } else if (affected_count > 0) {
    result.message = std::format("Imported {} file(s), {} failed, {} missing, {} skipped",
                                 affected_count, failed_count, not_found_count, unchanged_count);
  } else {
    result.message = std::format("No files were imported: {} failed, {} missing, {} skipped",
                                 failed_count, not_found_count, unchanged_count);
  }

  for (const auto& error : errors) {
    Logger().warn("Importer::import_files_to_folder: {}", error);
  }
  return result;
}

}  // namespace features::gallery::importer
