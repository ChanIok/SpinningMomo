module;

module Features.Gallery.Clipboard;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Scanner;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Watcher;
import Utils.Image;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.System;

namespace Features::Gallery::Clipboard {

std::atomic<std::uint64_t> clipboard_temp_file_sequence = 0;

// 在目标目录内生成专用临时路径，让 watcher 不会把尚未完成的文件当作媒体。
auto make_clipboard_temp_path(const std::filesystem::path& target_folder)
    -> std::expected<std::filesystem::path, std::string> {
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto sequence = clipboard_temp_file_sequence.fetch_add(1, std::memory_order_relaxed);
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    auto candidate = target_folder / std::format(".spinningmomo-paste-{}-{}.tmp", ticks, sequence);

    std::error_code exists_error;
    if (!std::filesystem::exists(candidate, exists_error)) {
      if (exists_error) {
        return std::unexpected("Failed to inspect clipboard temporary path: " +
                               exists_error.message());
      }
      return candidate;
    }
  }
  return std::unexpected("Failed to allocate a unique clipboard temporary path");
}

// 为保留原文件名的粘贴生成不覆盖目标，并在重名时追加递增序号。
auto make_unique_clipboard_destination(const std::filesystem::path& target_folder,
                                       const std::filesystem::path& requested_name,
                                       std::uint64_t suffix)
    -> std::expected<std::filesystem::path, std::string> {
  auto filename = requested_name.filename();
  if (filename.empty() || filename == L"." || filename == L"..") {
    return std::unexpected("Clipboard file name is invalid");
  }

  auto candidate = target_folder / filename;
  if (suffix > 0) {
    candidate =
        target_folder / std::filesystem::path(std::format(L"{} ({}){}", filename.stem().wstring(),
                                                          suffix, filename.extension().wstring()));
  }

  auto normalized_result = Utils::Path::NormalizePath(candidate);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize clipboard destination: " +
                           normalized_result.error());
  }
  if (!Utils::Path::IsPathWithinBase(normalized_result.value(), target_folder)) {
    return std::unexpected("Clipboard destination escaped the target folder");
  }
  return normalized_result.value();
}

// 将完整临时文件原子提交到首个空闲名称，绝不覆盖目标目录中的现有文件。
auto commit_clipboard_temp_file(const std::filesystem::path& temporary_path,
                                const std::filesystem::path& target_folder,
                                const std::filesystem::path& requested_name)
    -> std::expected<std::filesystem::path, std::string> {
  for (std::uint64_t suffix = 0; suffix < 10'000; ++suffix) {
    auto destination_result =
        make_unique_clipboard_destination(target_folder, requested_name, suffix);
    if (!destination_result) {
      return std::unexpected(destination_result.error());
    }
    const auto& destination = destination_result.value();

    std::error_code exists_error;
    if (std::filesystem::exists(destination, exists_error)) {
      continue;
    }
    if (exists_error) {
      return std::unexpected("Failed to inspect clipboard destination: " + exists_error.message());
    }

    // rename 在同一目录内提交完整文件；并发创建同名目标时继续尝试下一个名称。
    std::error_code rename_error;
    std::filesystem::rename(temporary_path, destination, rename_error);
    if (!rename_error) {
      return destination;
    }

    std::error_code collision_error;
    if (std::filesystem::exists(destination, collision_error) && !collision_error) {
      continue;
    }
    return std::unexpected("Failed to commit clipboard file: " + rename_error.message());
  }
  return std::unexpected("Too many clipboard file name collisions");
}

// 把编码字节完整写入临时文件，失败时不留下可被图库识别的媒体扩展名。
auto write_clipboard_bytes(const std::filesystem::path& path,
                           const std::vector<std::uint8_t>& bytes)
    -> std::expected<void, std::string> {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    return std::unexpected("Failed to create clipboard temporary file");
  }
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  stream.close();
  if (!stream) {
    return std::unexpected("Failed to write clipboard temporary file");
  }
  return {};
}

// 删除本次尚未提交的临时文件，不影响已经原子改名成功的目标。
auto cleanup_clipboard_temp_file(const std::filesystem::path& temporary_path) -> void {
  if (temporary_path.empty()) {
    return;
  }
  std::error_code cleanup_error;
  std::filesystem::remove(temporary_path, cleanup_error);
  if (cleanup_error) {
    Logger().warn("Failed to clean clipboard temporary file '{}': {}", temporary_path.string(),
                  cleanup_error.message());
  }
}

// 将选中资产解析为真实文件并写入系统剪贴板。
auto copy_assets(Core::State::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<Types::OperationResult, std::string> {
  // 这一层负责把“选中的资产 ID”转换成真正可复制的磁盘文件路径。
  // 真正的系统剪贴板写入由 Utils::System 负责。
  if (ids.empty()) {
    return Types::OperationResult{
        .success = false,
        .message = "No assets selected",
        .affected_count = 0,
    };
  }

  std::vector<std::int64_t> unique_ids;
  unique_ids.reserve(ids.size());
  std::unordered_set<std::int64_t> seen_ids;
  seen_ids.reserve(ids.size());

  // 保持选择集顺序，同时去掉重复 ID，避免重复复制同一个文件。
  for (auto id : ids) {
    if (seen_ids.insert(id).second) {
      unique_ids.push_back(id);
    }
  }

  std::vector<std::filesystem::path> clipboard_paths;
  clipboard_paths.reserve(unique_ids.size());

  std::int64_t copied_count = 0;
  std::int64_t not_found_count = 0;
  std::vector<std::string> errors;
  errors.reserve(unique_ids.size());

  // 逐个把资产 ID 转成文件路径，并过滤掉索引不存在或磁盘不存在的项。
  for (auto id : unique_ids) {
    auto asset_result = Asset::Repository::get_asset_by_id(app_state, id);
    if (!asset_result) {
      errors.push_back("Failed to query asset " + std::to_string(id) + ": " + asset_result.error());
      continue;
    }

    if (!asset_result->has_value()) {
      not_found_count++;
      continue;
    }

    const auto& asset = asset_result->value();
    if (asset.path.empty()) {
      errors.push_back("Asset path is empty for asset " + std::to_string(asset.id));
      continue;
    }

    std::filesystem::path file_path(asset.path);
    std::error_code ec;
    const bool file_exists = std::filesystem::exists(file_path, ec);
    if (ec) {
      errors.push_back("Failed to access file " + asset.path + ": " + ec.message());
      continue;
    }

    if (!file_exists) {
      not_found_count++;
      continue;
    }

    clipboard_paths.push_back(std::move(file_path));
  }

  // 只有在至少找到一个真实文件时，才真正写入系统剪贴板。
  if (!clipboard_paths.empty()) {
    auto copy_result = Utils::System::copy_files_to_clipboard(clipboard_paths);
    if (!copy_result) {
      errors.push_back("Failed to copy files to clipboard: " + copy_result.error());
    } else {
      copied_count = static_cast<std::int64_t>(clipboard_paths.size());
    }
  }

  const auto total_count = static_cast<std::int64_t>(unique_ids.size());
  const auto failed_count = std::max<std::int64_t>(0, total_count - copied_count - not_found_count);

  // 这里沿用 gallery 现有的 OperationResult 风格，
  // 方便前端统一做 success / partial / failed 的 toast 提示。
  Types::OperationResult result{
      .success = copied_count == total_count,
      .message = "",
      .affected_count = copied_count,
      .failed_count = failed_count,
      .not_found_count = not_found_count,
      .unchanged_count = 0,
  };

  if (result.success) {
    result.message = std::format("Copied {} asset(s) to clipboard", copied_count);
    return result;
  }

  if (copied_count > 0) {
    result.message = std::format("Copied {} asset(s) to clipboard, {} failed, {} not found",
                                 copied_count, failed_count, not_found_count);
  } else {
    result.message = std::format("Failed to copy assets to clipboard: {} failed, {} not found",
                                 failed_count, not_found_count);
  }

  // 详细错误记日志，用户界面只展示汇总结果即可。
  for (const auto& error : errors) {
    Logger().warn("Clipboard::copy_assets: {}", error);
  }

  return result;
}

// 将剪贴板文件或截图无覆盖地写入指定图库目录，再同步建立资产索引。
auto paste_to_folder(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<Types::OperationResult, std::string> {
  if (folder_id <= 0) {
    return Types::OperationResult{
        .success = false,
        .message = "Invalid target folder",
        .affected_count = 0,
    };
  }

  // 目标路径只从已索引 folder_id 解析，RPC 调用方不能指定任意磁盘位置。
  auto folder_result = Folder::Repository::get_folder_by_id(app_state, folder_id);
  if (!folder_result) {
    return std::unexpected("Failed to query clipboard target folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return Types::OperationResult{
        .success = false,
        .message = "Target folder not found",
        .affected_count = 0,
    };
  }

  auto target_result =
      Utils::Path::NormalizePath(std::filesystem::path(folder_result->value().path));
  if (!target_result) {
    return std::unexpected("Failed to normalize clipboard target folder: " + target_result.error());
  }
  const auto target_folder = target_result.value();

  std::error_code target_error;
  if (!std::filesystem::is_directory(target_folder, target_error)) {
    return Types::OperationResult{
        .success = false,
        .message = target_error ? "Failed to access target folder: " + target_error.message()
                                : "Target folder does not exist",
        .affected_count = 0,
    };
  }

  auto clipboard_result = Utils::System::read_clipboard_media();
  if (!clipboard_result) {
    return std::unexpected("Failed to read clipboard media: " + clipboard_result.error());
  }
  auto clipboard = std::move(clipboard_result.value());
  if (clipboard.kind == Utils::System::ClipboardMediaKind::Empty) {
    return Types::OperationResult{
        .success = false,
        .message = "Clipboard does not contain supported media",
        .affected_count = 0,
    };
  }

  std::vector<std::filesystem::path> created_paths;
  std::vector<Types::ScanChange> indexed_changes;
  std::int64_t not_found_count = 0;
  std::int64_t unchanged_count = 0;
  std::int64_t indexed_count = 0;
  std::vector<std::string> errors;

  // 文件列表逐项复制，单个失败不阻断同一批次中的其他媒体。
  if (clipboard.kind == Utils::System::ClipboardMediaKind::Files) {
    created_paths.reserve(clipboard.file_paths.size());
    for (const auto& source : clipboard.file_paths) {
      auto normalized_source_result = Utils::Path::NormalizePath(source);
      if (!normalized_source_result) {
        errors.push_back("Failed to normalize clipboard source '" + source.string() +
                         "': " + normalized_source_result.error());
        continue;
      }
      const auto normalized_source = normalized_source_result.value();

      std::error_code source_error;
      const bool source_exists = std::filesystem::exists(normalized_source, source_error);
      if (source_error) {
        errors.push_back("Failed to access clipboard source '" + normalized_source.string() +
                         "': " + source_error.message());
        continue;
      }
      if (!source_exists) {
        not_found_count++;
        continue;
      }
      const bool is_regular_file =
          std::filesystem::is_regular_file(normalized_source, source_error);
      if (source_error) {
        errors.push_back("Failed to inspect clipboard source '" + normalized_source.string() +
                         "': " + source_error.message());
        continue;
      }
      if (!is_regular_file) {
        unchanged_count++;
        continue;
      }

      const auto& supported_extensions = Scanner::Common::default_supported_extensions();
      if (!Scanner::Common::is_supported_file(normalized_source, supported_extensions)) {
        unchanged_count++;
        continue;
      }

      // 同目录粘贴不制造自身副本，避免把“导入”变成隐式复制命令。
      if (Utils::Path::NormalizeForComparison(normalized_source.parent_path()) ==
          Utils::Path::NormalizeForComparison(target_folder)) {
        unchanged_count++;
        continue;
      }

      auto temporary_result = make_clipboard_temp_path(target_folder);
      if (!temporary_result) {
        errors.push_back(temporary_result.error());
        continue;
      }
      auto temporary_path = temporary_result.value();

      std::error_code copy_error;
      std::filesystem::copy_file(normalized_source, temporary_path,
                                 std::filesystem::copy_options::none, copy_error);
      if (copy_error) {
        cleanup_clipboard_temp_file(temporary_path);
        errors.push_back("Failed to copy clipboard file '" + normalized_source.string() +
                         "': " + copy_error.message());
        continue;
      }

      auto commit_result =
          commit_clipboard_temp_file(temporary_path, target_folder, normalized_source.filename());
      if (!commit_result) {
        cleanup_clipboard_temp_file(temporary_path);
        errors.push_back(commit_result.error());
        continue;
      }
      created_paths.push_back(std::move(commit_result.value()));
    }
  } else {
    // 位图剪贴板只产生一个无损 PNG，并沿用截图文件名格式。
    auto temporary_result = make_clipboard_temp_path(target_folder);
    if (!temporary_result) {
      return std::unexpected(temporary_result.error());
    }
    auto temporary_path = temporary_result.value();

    std::expected<void, std::string> write_result;
    if (clipboard.kind == Utils::System::ClipboardMediaKind::EncodedPng) {
      write_result = write_clipboard_bytes(temporary_path, clipboard.encoded_png);
    } else if (clipboard.kind == Utils::System::ClipboardMediaKind::Bitmap &&
               clipboard.bitmap.has_value()) {
      auto factory_result = Utils::Image::get_thread_wic_factory();
      if (!factory_result) {
        cleanup_clipboard_temp_file(temporary_path);
        return std::unexpected("Failed to create image encoder: " + factory_result.error());
      }
      const auto& bitmap = clipboard.bitmap.value();
      write_result = Utils::Image::save_pixel_data_to_file(
          factory_result->get(), bitmap.bgra_pixels.data(), bitmap.width, bitmap.height,
          bitmap.stride, temporary_path.wstring(), Utils::Image::ImageFormat::PNG);
    } else {
      cleanup_clipboard_temp_file(temporary_path);
      return std::unexpected("Clipboard bitmap payload is missing");
    }

    if (!write_result) {
      cleanup_clipboard_temp_file(temporary_path);
      return std::unexpected("Failed to save clipboard image: " + write_result.error());
    }

    auto requested_name = std::filesystem::path(
        Utils::String::FromUtf8(Utils::String::FormatTimestamp(std::chrono::system_clock::now())));
    auto commit_result = commit_clipboard_temp_file(temporary_path, target_folder, requested_name);
    if (!commit_result) {
      cleanup_clipboard_temp_file(temporary_path);
      return std::unexpected(commit_result.error());
    }
    created_paths.push_back(std::move(commit_result.value()));
  }

  // 主动创建由 Gallery 同步建立索引，watcher 只负责随后可能到达的外部文件通知。
  for (const auto& created_path : created_paths) {
    auto index_result = Scanner::upsert_created_file(app_state, folder_id, created_path);
    if (!index_result) {
      errors.push_back("Failed to index pasted file '" + created_path.string() +
                       "': " + index_result.error());
      continue;
    }

    auto asset_result = Asset::Repository::get_asset_by_path(app_state, created_path.string());
    if (!asset_result) {
      errors.push_back("Failed to verify pasted file index '" + created_path.string() +
                       "': " + asset_result.error());
      continue;
    }
    if (!asset_result->has_value()) {
      unchanged_count++;
      continue;
    }

    indexed_count++;
    indexed_changes.insert(indexed_changes.end(),
                           std::make_move_iterator(index_result->changes.begin()),
                           std::make_move_iterator(index_result->changes.end()));
  }

  // 索引已经落库后再把真实 UPSERT 分发给扩展消费者。
  if (!indexed_changes.empty()) {
    auto dispatch_result = Watcher::dispatch_manual_scan_changes(app_state, indexed_changes);
    if (!dispatch_result) {
      errors.push_back("Failed to dispatch pasted file changes: " + dispatch_result.error());
    }
  }

  const auto affected_count = indexed_count;
  const auto failed_count = static_cast<std::int64_t>(errors.size());
  Types::OperationResult result{
      .success = affected_count > 0 && errors.empty(),
      .message = "",
      .affected_count = affected_count,
      .failed_count = failed_count,
      .not_found_count = not_found_count,
      .unchanged_count = unchanged_count,
  };

  if (result.success) {
    result.message = std::format("Pasted {} clipboard file(s)", affected_count);
  } else if (affected_count > 0) {
    result.message = std::format("Pasted {} clipboard file(s), {} failed, {} missing, {} skipped",
                                 affected_count, failed_count, not_found_count, unchanged_count);
  } else {
    result.message =
        std::format("No clipboard files were pasted: {} failed, {} missing, {} skipped",
                    failed_count, not_found_count, unchanged_count);
  }

  for (const auto& error : errors) {
    Logger().warn("Clipboard::paste_to_folder: {}", error);
  }
  return result;
}

}  // namespace Features::Gallery::Clipboard
