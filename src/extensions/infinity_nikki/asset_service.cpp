module;

module Extensions.InfinityNikki.AssetService;

import std;
import Core.Database;
import Core.Database.Types;
import Core.State;
import Extensions.InfinityNikki.MetadataDict;
import Extensions.InfinityNikki.Types;
import Extensions.InfinityNikki.WorldArea;
import Features.Gallery.Asset.QuerySupport;
import Features.Gallery.Asset.Repository;
import Features.Gallery.Types;
import Utils.Logger;
import <asio.hpp>;

namespace Extensions::InfinityNikki::AssetService {

auto trim_ascii_copy(std::string_view value) -> std::string {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

  std::size_t start = 0;
  while (start < value.size() && is_space(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && is_space(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

struct PhotoMapPointWithWorldRecord {
  std::int64_t asset_id;
  std::string name;
  std::optional<std::string> hash;
  std::optional<std::int64_t> file_created_at;
  double nikki_loc_x;
  double nikki_loc_y;
  std::optional<double> nikki_loc_z;
  std::int64_t asset_index;
  std::optional<std::string> user_world_id;
};

struct InfinityNikkiUserRecordRow {
  std::string record_key;
  std::string record_value;
};

struct SameOutfitDyeCodeFillStatsRow {
  std::int64_t matched_count = 0;
  std::int64_t fillable_count = 0;
  std::int64_t recorded_count = 0;
};

struct InfinityNikkiSourceOutfitDyeStateRow {
  std::optional<std::string> nikki_diy_json;
  std::int64_t clothes_count = 0;
};

struct AssetIdRow {
  std::int64_t asset_id = 0;
};

auto read_user_record(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<InfinityNikkiUserRecord, std::string> {
  auto rows_result = Core::Database::query<InfinityNikkiUserRecordRow>(*app_state.database,
                                                                       R"(
        SELECT record_key,
               record_value
        FROM asset_infinity_nikki_user_record
        WHERE asset_id = ?
      )",
                                                                       {asset_id});
  if (!rows_result) {
    return std::unexpected("Failed to query Infinity Nikki user record: " + rows_result.error());
  }

  InfinityNikkiUserRecord record;
  for (const auto& row : rows_result.value()) {
    if (row.record_key == "dye_code") {
      record.dye_code = row.record_value;
    } else if (row.record_key == "world_id") {
      record.world_id = WorldArea::normalize_world_id(row.record_value);
    }
  }

  return record;
}

auto has_user_record_value(const InfinityNikkiUserRecord& record) -> bool {
  return record.dye_code.has_value() || record.world_id.has_value();
}

// 根据照片的游戏坐标和用户记录，解析出所属地图区域。
// 优先使用用户手动设置的 world_id，否则根据坐标自动推断。
auto build_map_area(const InfinityNikkiMapConfig& map_config,
                    const std::optional<InfinityNikkiExtractedParams>& extracted,
                    const InfinityNikkiUserRecord& user_record)
    -> std::optional<InfinityNikkiMapArea> {
  if (!extracted.has_value() || !extracted->nikki_loc_x.has_value() ||
      !extracted->nikki_loc_y.has_value()) {
    return std::nullopt;
  }

  const WorldArea::GamePoint game_point{
      .x = *extracted->nikki_loc_x,
      .y = *extracted->nikki_loc_y,
      .z = extracted->nikki_loc_z,
  };
  const auto* auto_world = WorldArea::resolve_world_or_default(map_config, game_point);
  if (auto_world == nullptr) {
    return std::nullopt;
  }

  // 用户手动设置的 world_id 优先于自动推断结果。
  std::optional<std::string> user_world_id;
  const auto* world = auto_world;
  if (user_record.world_id.has_value()) {
    if (const auto* user_world = WorldArea::find_world(map_config, *user_record.world_id);
        user_world != nullptr) {
      user_world_id = user_world->world_id;
      world = user_world;
    }
  }

  return InfinityNikkiMapArea{
      .auto_world_id = auto_world->world_id,
      .auto_official_world_id = auto_world->official_world_id,
      .user_world_id = user_world_id,
      .world_id = world->world_id,
      .official_world_id = world->official_world_id,
  };
}

auto upsert_user_record_key(Core::State::AppState& app_state, std::int64_t asset_id,
                            std::string_view record_key, std::string_view record_value)
    -> std::expected<void, std::string> {
  auto result =
      Core::Database::execute(*app_state.database,
                              R"(
      INSERT INTO asset_infinity_nikki_user_record (asset_id, record_key, record_value)
      VALUES (?, ?, ?)
      ON CONFLICT(asset_id, record_key) DO UPDATE SET
        record_value = excluded.record_value
    )",
                              {asset_id, std::string(record_key), std::string(record_value)});
  if (!result) {
    return std::unexpected("Failed to upsert Infinity Nikki user record key: " + result.error());
  }
  return {};
}

auto delete_user_record_keys(Core::State::AppState& app_state, std::int64_t asset_id,
                             const std::vector<std::string>& record_keys)
    -> std::expected<void, std::string> {
  for (const auto& record_key : record_keys) {
    auto result = Core::Database::execute(
        *app_state.database,
        "DELETE FROM asset_infinity_nikki_user_record WHERE asset_id = ? AND record_key = ?",
        {asset_id, record_key});
    if (!result) {
      return std::unexpected("Failed to delete Infinity Nikki user record key: " + result.error());
    }
  }
  return {};
}

auto db_param_from_optional_string(const std::optional<std::string>& value)
    -> Core::Database::Types::DbParam {
  if (!value.has_value()) {
    return std::monostate{};
  }
  return *value;
}

auto read_source_outfit_dye_state(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::optional<InfinityNikkiSourceOutfitDyeStateRow>, std::string> {
  auto result =
      Core::Database::query_single<InfinityNikkiSourceOutfitDyeStateRow>(*app_state.database,
                                                                         R"(
        SELECT p.nikki_diy_json AS nikki_diy_json,
               (
                 SELECT COUNT(*)
                 FROM asset_infinity_nikki_clothes c
                 WHERE c.asset_id = p.asset_id
               ) AS clothes_count
        FROM asset_infinity_nikki_params p
        WHERE p.asset_id = ?
      )",
                                                                         {asset_id});
  if (!result) {
    return std::unexpected("Failed to query Infinity Nikki outfit and dye state: " +
                           result.error());
  }

  return result.value();
}

auto query_same_outfit_dye_code_fill_preview(
    Core::State::AppState& app_state, std::int64_t source_asset_id,
    const InfinityNikkiSourceOutfitDyeStateRow& source_state)
    -> std::expected<InfinityNikkiSameOutfitDyeCodeFillPreview, std::string> {
  const auto diy_json_param = db_param_from_optional_string(source_state.nikki_diy_json);
  auto stats_result = Core::Database::query_single<SameOutfitDyeCodeFillStatsRow>(
      *app_state.database,
      R"(
        SELECT COUNT(*) AS matched_count,
               COALESCE(SUM(
                 CASE
                   WHEN ur.asset_id IS NULL OR trim(ur.record_value) = '' THEN 1
                   ELSE 0
                 END
               ), 0) AS fillable_count,
               COALESCE(SUM(
                 CASE
                   WHEN ur.asset_id IS NOT NULL AND trim(ur.record_value) <> '' THEN 1
                   ELSE 0
                 END
               ), 0) AS recorded_count
        FROM asset_infinity_nikki_params p
        LEFT JOIN asset_infinity_nikki_user_record ur
          ON ur.asset_id = p.asset_id
         AND ur.record_key = 'dye_code'
        WHERE p.asset_id <> ?
          AND ((? IS NULL AND p.nikki_diy_json IS NULL) OR p.nikki_diy_json = ?)
          AND (
            SELECT COUNT(*)
            FROM asset_infinity_nikki_clothes c
            WHERE c.asset_id = p.asset_id
          ) = ?
          AND NOT EXISTS (
            SELECT 1
            FROM asset_infinity_nikki_clothes sc
            WHERE sc.asset_id = ?
              AND NOT EXISTS (
                SELECT 1
                FROM asset_infinity_nikki_clothes cc
                WHERE cc.asset_id = p.asset_id
                  AND cc.cloth_id = sc.cloth_id
              )
          )
      )",
      {source_asset_id, diy_json_param, diy_json_param, source_state.clothes_count,
       source_asset_id});
  if (!stats_result) {
    return std::unexpected("Failed to count same Infinity Nikki outfit and dye assets: " +
                           stats_result.error());
  }

  const auto stats = stats_result->value_or(SameOutfitDyeCodeFillStatsRow{});
  return InfinityNikkiSameOutfitDyeCodeFillPreview{
      .source_has_outfit_dye_state = true,
      .matched_count = stats.matched_count,
      .fillable_count = stats.fillable_count,
      .recorded_count = stats.recorded_count,
  };
}

auto query_same_outfit_dye_code_target_asset_ids(
    Core::State::AppState& app_state, std::int64_t source_asset_id,
    const InfinityNikkiSourceOutfitDyeStateRow& source_state)
    -> std::expected<std::vector<std::int64_t>, std::string> {
  const auto diy_json_param = db_param_from_optional_string(source_state.nikki_diy_json);
  auto rows_result =
      Core::Database::query<AssetIdRow>(*app_state.database,
                                        R"(
        SELECT p.asset_id AS asset_id
        FROM asset_infinity_nikki_params p
        WHERE p.asset_id <> ?
          AND ((? IS NULL AND p.nikki_diy_json IS NULL) OR p.nikki_diy_json = ?)
          AND (
            SELECT COUNT(*)
            FROM asset_infinity_nikki_clothes c
            WHERE c.asset_id = p.asset_id
          ) = ?
          AND NOT EXISTS (
            SELECT 1
            FROM asset_infinity_nikki_clothes sc
            WHERE sc.asset_id = ?
              AND NOT EXISTS (
                SELECT 1
                FROM asset_infinity_nikki_clothes cc
                WHERE cc.asset_id = p.asset_id
                  AND cc.cloth_id = sc.cloth_id
              )
          )
      )",
                                        {source_asset_id, diy_json_param, diy_json_param,
                                         source_state.clothes_count, source_asset_id});
  if (!rows_result) {
    return std::unexpected("Failed to query same Infinity Nikki outfit and dye assets: " +
                           rows_result.error());
  }

  std::vector<std::int64_t> asset_ids;
  asset_ids.reserve(rows_result->size());
  for (const auto& row : rows_result.value()) {
    asset_ids.push_back(row.asset_id);
  }
  return asset_ids;
}

// 查询指定世界下的照片地图点位。
// 加载远端地图配置后，对每个点位进行世界归属判定和坐标变换，
// 只返回属于请求 world_id 的点位，且每个点位已包含 lat/lng。
auto query_photo_map_points(Core::State::AppState& app_state,
                            const QueryPhotoMapPointsParams& params)
    -> asio::awaitable<std::expected<std::vector<PhotoMapPoint>, std::string>> {
  auto where_result =
      Features::Gallery::Asset::QuerySupport::build_unified_where_clause(params.filters, "a");
  if (!where_result) {
    co_return std::unexpected(where_result.error());
  }
  auto [where_clause, query_params] = std::move(where_result.value());

  auto order_config = Features::Gallery::Asset::QuerySupport::build_query_order_config(
      params.sort_by, params.sort_order);

  std::string sql = std::format(R"(
    WITH filtered_assets AS (
      SELECT a.id,
             a.name,
             a.hash,
             a.file_created_at,
             a.type AS asset_type,

             COALESCE(a.file_created_at, a.created_at) AS sort_created_at,
             a.file_created_at AS sort_file_created_at,
             a.name AS sort_name,
             (COALESCE(a.width, 0) * COALESCE(a.height, 0)) AS sort_resolution,
             COALESCE(a.width, 0) AS sort_width,
             COALESCE(a.height, 0) AS sort_height,
             a.size AS sort_size
      FROM assets a
      {}
    ),
    indexed_assets AS (
      SELECT id,
             ROW_NUMBER() OVER ({}) - 1 AS asset_index
      FROM filtered_assets
    )
    SELECT fa.id AS asset_id,
           fa.name,
           fa.hash,
           fa.file_created_at,
           p.nikki_loc_x,
           p.nikki_loc_y,
           p.nikki_loc_z,
           ia.asset_index AS asset_index,
           wr.record_value AS user_world_id
    FROM indexed_assets ia
    INNER JOIN filtered_assets fa ON fa.id = ia.id
    INNER JOIN asset_infinity_nikki_params p ON p.asset_id = ia.id
    LEFT JOIN asset_infinity_nikki_user_record wr
      ON wr.asset_id = ia.id
     AND wr.record_key = 'world_id'
    WHERE p.nikki_loc_x IS NOT NULL
      AND p.nikki_loc_y IS NOT NULL
      AND fa.asset_type IN ('photo', 'live_photo')
    ORDER BY ia.asset_index
  )",
                                where_clause, order_config.indexed_order_clause);

  auto result =
      Core::Database::query<PhotoMapPointWithWorldRecord>(*app_state.database, sql, query_params);
  if (!result) {
    co_return std::unexpected("Failed to query photo map points: " + result.error());
  }

  const auto world_id = WorldArea::normalize_world_id(params.world_id);
  if (world_id.empty()) {
    co_return std::vector<PhotoMapPoint>{};
  }

  auto map_config_result = co_await WorldArea::load_map_config(app_state);
  if (!map_config_result) {
    co_return std::unexpected(map_config_result.error());
  }
  const auto& map_config = map_config_result.value();
  const auto* requested_world = WorldArea::find_world(map_config, world_id);
  if (requested_world == nullptr) {
    co_return std::vector<PhotoMapPoint>{};
  }

  std::vector<PhotoMapPoint> filtered_points;
  filtered_points.reserve(result->size());

  for (const auto& point : result.value()) {
    const WorldArea::GamePoint game_point{
        .x = point.nikki_loc_x,
        .y = point.nikki_loc_y,
        .z = point.nikki_loc_z,
    };

    // 世界归属判定：优先使用用户手动设置的 world_id，否则根据坐标自动推断。
    const auto* resolved_world = point.user_world_id.has_value()
                                     ? WorldArea::find_world(map_config, *point.user_world_id)
                                     : nullptr;
    if (resolved_world == nullptr) {
      resolved_world = WorldArea::resolve_world_or_default(map_config, game_point);
    }
    // 只保留属于请求世界的点位。
    if (resolved_world == nullptr || resolved_world->world_id != requested_world->world_id) {
      continue;
    }

    // 将游戏坐标变换为地图经纬度，前端可直接用于标记点位。
    const auto map_coordinate =
        WorldArea::transform_game_to_map_coordinates(game_point, *resolved_world);
    if (!map_coordinate) {
      co_return std::unexpected(map_coordinate.error());
    }

    filtered_points.push_back(PhotoMapPoint{
        .asset_id = point.asset_id,
        .name = point.name,
        .hash = point.hash,
        .file_created_at = point.file_created_at,
        .nikki_loc_x = point.nikki_loc_x,
        .nikki_loc_y = point.nikki_loc_y,
        .nikki_loc_z = point.nikki_loc_z,
        .lat = map_coordinate->lat,
        .lng = map_coordinate->lng,
        .world_id = resolved_world->world_id,
        .official_world_id = resolved_world->official_world_id,
        .asset_index = point.asset_index,
    });
  }

  co_return filtered_points;
}

auto get_details(Core::State::AppState& app_state, const GetInfinityNikkiDetailsParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiDetails, std::string>> {
  std::string extracted_sql = R"(
    SELECT camera_params,
           time_hour,
           time_min,
           camera_focal_length,
           rotation,
           aperture_value,
           filter_id,
           filter_strength,
           vignette_intensity,
           light_id,
           light_strength,
           vertical,
           bloom_intensity,
           bloom_threshold,
           brightness,
           exposure,
           contrast,
           saturation,
           vibrance,
           highlights,
           shadow,
           nikki_loc_x,
           nikki_loc_y,
           nikki_loc_z,
           nikki_hidden,
           pose_id
    FROM asset_infinity_nikki_params
    WHERE asset_id = ?
  )";

  auto extracted_result = Core::Database::query_single<InfinityNikkiExtractedParams>(
      *app_state.database, extracted_sql, {params.asset_id});
  if (!extracted_result) {
    co_return std::unexpected("Failed to query Infinity Nikki extracted params: " +
                              extracted_result.error());
  }

  auto user_record_result = read_user_record(app_state, params.asset_id);
  if (!user_record_result) {
    co_return std::unexpected(user_record_result.error());
  }

  auto user_record = std::move(user_record_result.value());
  // 尝试加载地图配置以构建 map_area 增强信息；
  // 配置加载失败时降级：详情仍正常返回，只是不包含 map_area。
  std::optional<InfinityNikkiMapArea> map_area;
  const auto& extracted = extracted_result.value();
  if (extracted.has_value() && extracted->nikki_loc_x.has_value() &&
      extracted->nikki_loc_y.has_value()) {
    auto map_config_result = co_await WorldArea::load_map_config(app_state);
    if (map_config_result) {
      map_area = build_map_area(map_config_result.value(), extracted, user_record);
    } else {
      Logger().warn("Skip Infinity Nikki map area details because map config failed: {}",
                    map_config_result.error());
    }
  }

  auto user_record_optional = has_user_record_value(user_record)
                                  ? std::optional<InfinityNikkiUserRecord>{user_record}
                                  : std::nullopt;

  co_return InfinityNikkiDetails{.extracted = extracted_result.value(),
                                 .user_record = std::move(user_record_optional),
                                 .map_area = std::move(map_area)};
}

// 暴露给 RPC 层的地图配置获取入口，供前端直接调用。
auto get_map_config(Core::State::AppState& app_state)
    -> asio::awaitable<std::expected<InfinityNikkiMapConfig, std::string>> {
  co_return co_await WorldArea::load_map_config(app_state);
}

auto get_metadata_names(Core::State::AppState& app_state,
                        const GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiMetadataNames, std::string>> {
  co_return co_await MetadataDict::resolve_metadata_names(app_state, params);
}

auto set_user_record(Core::State::AppState& app_state,
                     const SetInfinityNikkiUserRecordParams& params)
    -> std::expected<Features::Gallery::Types::OperationResult, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before updating Infinity Nikki user record: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto normalized_code_value =
      params.code_value.has_value()
          ? std::optional<std::string>{trim_ascii_copy(params.code_value.value())}
          : std::nullopt;
  if (normalized_code_value.has_value() && normalized_code_value->empty()) {
    normalized_code_value = std::nullopt;
  }

  auto write_result = Core::Database::execute_transaction(
      *app_state.database, [&](auto&) -> std::expected<void, std::string> {
        if (normalized_code_value.has_value()) {
          auto value_result = upsert_user_record_key(app_state, params.asset_id, "dye_code",
                                                     normalized_code_value.value());
          if (!value_result) {
            return std::unexpected(value_result.error());
          }
          return {};
        }

        return delete_user_record_keys(app_state, params.asset_id, {"dye_code"});
      });

  if (!write_result) {
    return std::unexpected(write_result.error());
  }

  return Features::Gallery::Types::OperationResult{
      .success = true,
      .message = "Infinity Nikki user record updated successfully",
      .affected_count = 1};
}

auto preview_same_outfit_dye_code_fill(
    Core::State::AppState& app_state, const PreviewInfinityNikkiSameOutfitDyeCodeFillParams& params)
    -> std::expected<InfinityNikkiSameOutfitDyeCodeFillPreview, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before previewing same outfit and dye fill: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto source_state_result = read_source_outfit_dye_state(app_state, params.asset_id);
  if (!source_state_result) {
    return std::unexpected(source_state_result.error());
  }

  if (!source_state_result->has_value()) {
    return InfinityNikkiSameOutfitDyeCodeFillPreview{};
  }

  const auto& source_state = source_state_result->value();
  if (source_state.clothes_count <= 0) {
    return InfinityNikkiSameOutfitDyeCodeFillPreview{};
  }

  return query_same_outfit_dye_code_fill_preview(app_state, params.asset_id, source_state);
}

auto fill_same_outfit_dye_code(Core::State::AppState& app_state,
                               const FillInfinityNikkiSameOutfitDyeCodeParams& params)
    -> std::expected<InfinityNikkiSameOutfitDyeCodeFillResult, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto normalized_code_value = trim_ascii_copy(params.code_value);
  if (normalized_code_value.empty()) {
    return std::unexpected("Dye code must not be empty");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before filling same outfit and dye records: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto source_state_result = read_source_outfit_dye_state(app_state, params.asset_id);
  if (!source_state_result) {
    return std::unexpected(source_state_result.error());
  }

  if (!source_state_result->has_value()) {
    return InfinityNikkiSameOutfitDyeCodeFillResult{
        .success = true,
        .message = "Source asset has no Infinity Nikki outfit data",
        .source_has_outfit_dye_state = false,
    };
  }

  const auto& source_state = source_state_result->value();
  if (source_state.clothes_count <= 0) {
    return InfinityNikkiSameOutfitDyeCodeFillResult{
        .success = true,
        .message = "Source asset has no Infinity Nikki outfit data",
        .source_has_outfit_dye_state = false,
    };
  }

  auto preview_result =
      query_same_outfit_dye_code_fill_preview(app_state, params.asset_id, source_state);
  if (!preview_result) {
    return std::unexpected(preview_result.error());
  }

  auto target_ids_result =
      query_same_outfit_dye_code_target_asset_ids(app_state, params.asset_id, source_state);
  if (!target_ids_result) {
    return std::unexpected(target_ids_result.error());
  }

  auto target_ids = std::move(target_ids_result.value());
  auto write_result = Core::Database::execute_transaction(
      *app_state.database, [&](auto&) -> std::expected<void, std::string> {
        for (const auto asset_id : target_ids) {
          auto value_result =
              upsert_user_record_key(app_state, asset_id, "dye_code", normalized_code_value);
          if (!value_result) {
            return std::unexpected(value_result.error());
          }
        }
        return {};
      });

  if (!write_result) {
    return std::unexpected(write_result.error());
  }

  return InfinityNikkiSameOutfitDyeCodeFillResult{
      .success = true,
      .message = "Infinity Nikki same outfit and dye records filled successfully",
      .source_has_outfit_dye_state = true,
      .matched_count = preview_result->matched_count,
      .affected_count = static_cast<std::int64_t>(target_ids.size()),
      .skipped_existing_count = 0,
      .updated_existing_count = preview_result->recorded_count,
  };
}

auto set_world_record(Core::State::AppState& app_state,
                      const SetInfinityNikkiWorldRecordParams& params)
    -> std::expected<Features::Gallery::Types::OperationResult, std::string> {
  if (params.asset_id <= 0) {
    return std::unexpected("Asset id must be greater than 0");
  }

  auto asset_result =
      Features::Gallery::Asset::Repository::get_asset_by_id(app_state, params.asset_id);
  if (!asset_result) {
    return std::unexpected("Failed to load asset before updating Infinity Nikki world record: " +
                           asset_result.error());
  }

  if (!asset_result->has_value()) {
    return std::unexpected("Asset not found");
  }

  auto normalized_world_id =
      params.world_id.has_value()
          ? std::optional<std::string>{WorldArea::normalize_world_id(params.world_id.value())}
          : std::nullopt;
  if (normalized_world_id.has_value() && normalized_world_id->empty()) {
    normalized_world_id = std::nullopt;
  }

  std::expected<void, std::string> write_result;
  if (normalized_world_id.has_value()) {
    write_result =
        upsert_user_record_key(app_state, params.asset_id, "world_id", normalized_world_id.value());
  } else {
    write_result = delete_user_record_keys(app_state, params.asset_id, {"world_id"});
  }

  if (!write_result) {
    return std::unexpected(write_result.error());
  }

  return Features::Gallery::Types::OperationResult{
      .success = true,
      .message = "Infinity Nikki world record updated successfully",
      .affected_count = 1};
}

}  // namespace Extensions::InfinityNikki::AssetService
