module;

module Features.Gallery.Scanner.AssetPipeline;

import std;
import Core.Database;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Scanner.Common;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Asset.Thumbnail;
import Features.Gallery.Color.Types;
import Features.Gallery.Color.Extractor;
import Features.Gallery.Color.Repository;
import Features.Gallery.Ignore.Service;
import Utils.Media.VideoAsset;
import Utils.Image;
import Utils.Logger;
import Utils.Path;
import Utils.String;
import Utils.Time;

namespace Features::Gallery::Scanner::AssetPipeline {

// 已知指纹与文件状态后：填 Asset + 缩略图/主色，不写库
auto prepare_media_asset(Core::State::AppState& app_state,
                         const std::filesystem::path& normalized_path,
                         const Types::ScanOptions& options, const MediaPrepareInput& input)
    -> std::expected<PreparedAsset, std::string> {
  auto asset_type = Common::detect_asset_type(normalized_path);

  PreparedAsset prepared;
  prepared.is_update = input.existing_asset_id.has_value();
  auto& asset = prepared.asset;
  // 更新只需要旧 ID；Repository 会限制为 Scanner-owned 字段，不需要回读用户字段。
  asset.id = input.existing_asset_id.value_or(0);
  asset.created_at = 0;
  asset.updated_at = 0;

  asset.name = normalized_path.filename().string();
  asset.path = normalized_path.string();
  asset.type = asset_type;
  asset.size = input.size;
  asset.hash = input.hash.empty() ? std::nullopt : std::optional<std::string>{input.hash};
  asset.file_created_at = input.file_created_millis;
  asset.file_modified_at = input.file_modified_millis;
  asset.folder_id = input.folder_id;
  asset.mime_type = "application/octet-stream";

  if (normalized_path.has_extension()) {
    asset.extension = Utils::String::ToLowerAscii(normalized_path.extension().string());
  } else {
    asset.extension = std::nullopt;
  }

  if (asset_type == "photo") {
    // 照片分支直接获取线程局部工厂：首次调用才初始化，后续照片自动复用。
    auto wic_factory_result = Utils::Image::get_thread_wic_factory();
    if (!wic_factory_result) {
      return std::unexpected("Failed to get thread WIC factory: " + wic_factory_result.error());
    }
    auto& photo_wic_factory = wic_factory_result.value();

    auto image_info_result = Utils::Image::get_image_info(photo_wic_factory.get(), normalized_path);
    if (image_info_result) {
      auto image_info = std::move(*image_info_result);
      asset.width = image_info.width;
      asset.height = image_info.height;
      asset.mime_type = std::move(image_info.mime_type);
    } else {
      Logger().warn("Could not extract image info from {}: {}", normalized_path.string(),
                    image_info_result.error());
      asset.width = 0;
      asset.height = 0;
      asset.mime_type = "application/octet-stream";
    }

    std::optional<Utils::Image::BGRABitmapData> thumbnail_bitmap_data;

    // 缩略图像素同时供主色复用，避免同一照片重复解码缩放
    auto bitmap_data_result = Utils::Image::load_scaled_bgra_bitmap_data(
        photo_wic_factory.get(), normalized_path, Types::kDefaultThumbnailShortEdge);
    if (!bitmap_data_result) {
      Logger().warn("Failed to load thumbnail bitmap data for {}: {}", normalized_path.string(),
                    bitmap_data_result.error());
    } else {
      thumbnail_bitmap_data = std::move(bitmap_data_result.value());

      if (input.hash.empty()) {
        Logger().warn("Skip thumbnail generation for {}: empty hash", normalized_path.string());
      } else {
        auto thumbnail_result = Asset::Thumbnail::save_thumbnail_from_bgra(
            app_state, input.hash, thumbnail_bitmap_data.value(),
            options.rebuild_thumbnails.value_or(false));
        if (!thumbnail_result) {
          Logger().warn("Failed to generate thumbnail for {}: {}", normalized_path.string(),
                        thumbnail_result.error());
        }
      }
    }

    const Features::Gallery::Color::Types::MainColorExtractOptions color_extract_options{
        .sample_short_edge = Types::kDefaultThumbnailShortEdge,
    };
    auto color_result = thumbnail_bitmap_data.has_value()
                            ? Features::Gallery::Color::Extractor::extract_main_colors_from_bgra(
                                  thumbnail_bitmap_data.value(), color_extract_options)
                            : Features::Gallery::Color::Extractor::extract_main_colors(
                                  photo_wic_factory, normalized_path, color_extract_options);
    if (color_result) {
      prepared.colors = std::move(color_result.value());
    } else {
      Logger().warn("Failed to extract main colors for {}: {}", normalized_path.string(),
                    color_result.error());
      prepared.colors.clear();
    }
  } else if (asset_type == "video") {
    // MF：分辨率/时长 + 封面；失败时兜底写入，避免单文件拖垮整批
    auto video_result = Utils::Media::VideoAsset::analyze_video_file(
        normalized_path, Types::kDefaultThumbnailShortEdge);
    if (video_result) {
      asset.width = static_cast<std::int32_t>(video_result->width);
      asset.height = static_cast<std::int32_t>(video_result->height);
      asset.mime_type = video_result->mime_type;

      if (video_result->thumbnail.has_value()) {
        if (input.hash.empty()) {
          Logger().warn("Skip video thumbnail save for {}: empty hash", normalized_path.string());
        } else {
          auto save_result =
              Asset::Thumbnail::save_thumbnail_data(app_state, input.hash, *video_result->thumbnail,
                                                    options.rebuild_thumbnails.value_or(false));
          if (!save_result) {
            Logger().warn("Failed to save video thumbnail for {}: {}", normalized_path.string(),
                          save_result.error());
          }
        }
      }
    } else {
      Logger().warn("Could not analyze video file {}: {}", normalized_path.string(),
                    video_result.error());
      asset.width = 0;
      asset.height = 0;
      asset.mime_type = "application/octet-stream";
    }

    prepared.colors.clear();
  } else {
    asset.width = 0;
    asset.height = 0;
    asset.mime_type = "application/octet-stream";
    prepared.colors.clear();
  }

  return prepared;
}

// 外部路径消失只进入 missing 宽限期；重复 REMOVE 不重置 missing_at。
auto mark_asset_missing_at_path(Core::State::AppState& app_state, const std::filesystem::path& path)
    -> std::expected<bool, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize path: " + normalized_result.error());
  }
  return Asset::Repository::mark_asset_missing_by_path(app_state, normalized_result->string());
}

// 在一个事务中写入单个资产及颜色，避免指纹已提交但颜色仍停留在旧状态。
auto persist_prepared_asset(Core::State::AppState& app_state, PreparedAsset& prepared)
    -> std::expected<PathSyncOutcome, std::string> {
  return Core::Database::execute_transaction(
      app_state,
      [&prepared](
          Core::State::AppState& txn_app_state) -> std::expected<PathSyncOutcome, std::string> {
        if (prepared.is_update) {
          auto update_result =
              Asset::Repository::update_asset_scanner_fields(txn_app_state, prepared.asset);
          if (!update_result) {
            return std::unexpected("Failed to update asset: " + update_result.error());
          }

          auto color_result =
              Features::Gallery::Color::Repository::replace_asset_colors_in_transaction(
                  txn_app_state, prepared.asset.id, prepared.colors);
          if (!color_result) {
            return std::unexpected("Failed to update asset colors: " + color_result.error());
          }
          return PathSyncOutcome::Updated;
        }

        auto create_result = Asset::Repository::create_asset_with_inherited_data_in_transaction(
            txn_app_state, prepared.asset);
        if (!create_result) {
          return std::unexpected("Failed to create asset: " + create_result.error());
        }
        prepared.asset.id = create_result.value();

        auto color_result =
            Features::Gallery::Color::Repository::replace_asset_colors_in_transaction(
                txn_app_state, prepared.asset.id, prepared.colors);
        if (!color_result) {
          return std::unexpected("Failed to create asset colors: " + color_result.error());
        }
        return PathSyncOutcome::Created;
      });
}

// 增量路径：过滤 → 粗判 → 指纹 → 媒体 → 单条写库
auto upsert_asset_at_path(Core::State::AppState& app_state, const std::filesystem::path& root_path,
                          const Types::ScanOptions& options,
                          const std::vector<Types::IgnoreRule>& ignore_rules,
                          const std::unordered_map<std::string, std::int64_t>& folder_mapping,
                          const std::filesystem::path& path, std::stop_token stop_token)
    -> std::expected<PathSyncOutcome, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize path: " + normalized_result.error());
  }
  auto normalized = normalized_result.value();

  std::error_code ec;
  // 盘上不存在或非常规文件 → 按删除处理
  if (!std::filesystem::exists(normalized, ec) ||
      !std::filesystem::is_regular_file(normalized, ec) || ec) {
    auto remove_result = mark_asset_missing_at_path(app_state, normalized);
    if (!remove_result) {
      return std::unexpected(remove_result.error());
    }
    return remove_result.value() ? PathSyncOutcome::Missing : PathSyncOutcome::Skipped;
  }

  const auto supported_extensions =
      options.supported_extensions.value_or(Common::default_supported_extensions());
  if (!Common::is_supported_file(normalized, supported_extensions)) {
    return PathSyncOutcome::Skipped;
  }

  if (Ignore::Service::apply_ignore_rules(normalized, root_path, ignore_rules, false)) {
    return PathSyncOutcome::Skipped;
  }

  auto file_size = std::filesystem::file_size(normalized, ec);
  if (ec) {
    return std::unexpected("Failed to read file size: " + ec.message());
  }

  auto last_write_time = std::filesystem::last_write_time(normalized, ec);
  if (ec) {
    return std::unexpected("Failed to read file modified time: " + ec.message());
  }

  auto creation_time_result = Utils::Time::get_file_creation_time_millis(normalized);
  if (!creation_time_result) {
    return std::unexpected("Failed to read file creation time: " + creation_time_result.error());
  }

  auto existing_result = Asset::Repository::get_asset_by_path(app_state, normalized.string());
  if (!existing_result) {
    return std::unexpected("Failed to query existing asset: " + existing_result.error());
  }

  auto existing_asset = existing_result.value();
  auto file_modified_millis = Utils::Time::file_time_to_millis(last_write_time);
  auto size_i64 = static_cast<std::int64_t>(file_size);

  // 相同路径始终代表同一资产；即使编辑器采用删除后替换，也先恢复原行。
  bool restored = false;
  if (existing_asset) {
    auto restore_result = Asset::Repository::restore_asset_by_id(app_state, existing_asset->id);
    if (!restore_result) {
      return std::unexpected(restore_result.error());
    }
    restored = restore_result.value();
  }

  // size/mtime 均一致则跳过内容指纹
  if (existing_asset && existing_asset->size.value_or(0) == size_i64 &&
      existing_asset->file_modified_at.value_or(0) == file_modified_millis) {
    return restored ? PathSyncOutcome::Restored : PathSyncOutcome::Skipped;
  }

  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  auto hash_result = Common::calculate_content_fingerprint(normalized, size_i64, stop_token);
  if (!hash_result) {
    return std::unexpected(hash_result.error());
  }
  auto hash = hash_result.value();

  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  // 指纹未变：只回写 size/mtime，避免反复读媒体
  if (existing_asset && existing_asset->hash.has_value() && !existing_asset->hash->empty() &&
      existing_asset->hash.value() == hash) {
    auto update_result = Asset::Repository::update_asset_file_state(app_state, existing_asset->id,
                                                                    size_i64, file_modified_millis);
    if (!update_result) {
      return std::unexpected(update_result.error());
    }
    return restored ? PathSyncOutcome::Restored : PathSyncOutcome::UnchangedMeta;
  }

  std::optional<std::int64_t> folder_id;
  auto parent_path = normalized.parent_path().string();
  if (auto folder_it = folder_mapping.find(parent_path); folder_it != folder_mapping.end()) {
    folder_id = folder_it->second;
  }

  MediaPrepareInput input{
      .hash = hash,
      .size = size_i64,
      .file_created_millis = creation_time_result.value(),
      .file_modified_millis = file_modified_millis,
      .folder_id = folder_id,
      .existing_asset_id =
          existing_asset ? std::optional<std::int64_t>{existing_asset->id} : std::nullopt,
  };

  auto prepared_result = prepare_media_asset(app_state, normalized, options, input);
  if (!prepared_result) {
    return std::unexpected(prepared_result.error());
  }

  if (stop_token.stop_requested()) {
    return std::unexpected("Gallery scan cancelled");
  }

  return persist_prepared_asset(app_state, prepared_result.value());
}

}  // namespace Features::Gallery::Scanner::AssetPipeline
