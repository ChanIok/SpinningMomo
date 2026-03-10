module;

#include <rfl/json.hpp>

module Plugins.InfinityNikki.PhotoExtract.Infra;

import std;
import Core.Database;
import Core.Database.Types;
import Core.State;
import Core.HttpClient;
import Core.HttpClient.Types;
import Plugins.InfinityNikki.PhotoExtract.Scan;
import <asio.hpp>;

namespace Plugins::InfinityNikki::PhotoExtract::Infra {

struct ExtractApiRequestBody {
  std::string uid;
  std::vector<std::string> params_base64_list;
};

struct PhotoTime {
  std::optional<std::int64_t> day;
  std::optional<std::int64_t> hour;
  std::optional<std::int64_t> min;
  std::optional<double> sec;
};

struct PhotoInfo {
  std::optional<double> camera_focal_length;
  std::optional<std::int64_t> aperture_section;
  std::optional<std::string> filter_id;
  std::optional<double> filter_strength;
  std::optional<double> vignette_intensity;
  std::optional<std::string> light_id;
  std::optional<double> light_strength;
  std::optional<double> nikki_loc_x;
  std::optional<double> nikki_loc_y;
  std::optional<double> nikki_loc_z;
  std::optional<bool> nikki_hidden;
  std::optional<std::int64_t> pose_id;
  rfl::Rename<"nikkiDIY", std::optional<rfl::Generic>> nikki_diy;
  rfl::Rename<"nikkiClothes", std::optional<std::vector<double>>> nikki_clothes;
};

struct SocialPhotoData {
  rfl::Rename<"CameraParams", std::optional<std::string>> camera_params;
  rfl::Rename<"Time", std::optional<PhotoTime>> time;
  rfl::Rename<"PhotoInfo", std::optional<PhotoInfo>> photo_info;
};

struct ApiError {
  std::optional<std::string> code;
  std::optional<std::string> message;
};

struct SocialPhotoWrapper {
  rfl::Rename<"SocialPhoto", std::optional<SocialPhotoData>> social_photo;
};

struct ApiResponse {
  bool success = false;
  std::optional<std::vector<std::optional<SocialPhotoWrapper>>> data;
  std::optional<ApiError> error;
};

constexpr std::string_view kExtractApiUrl = "https://api.infinitymomo.com/api/v1/extract";

auto to_parsed_record(const SocialPhotoData& sp) -> ParsedPhotoParamsRecord {
  ParsedPhotoParamsRecord record;
  record.camera_params = sp.camera_params.value();

  if (const auto& time = sp.time.value()) {
    record.time_day = time->day;
    record.time_hour = time->hour;
    record.time_min = time->min;
    record.time_sec = time->sec;
  }

  if (const auto& photo_info = sp.photo_info.value()) {
    const auto& pi = *photo_info;
    record.camera_focal_length = pi.camera_focal_length;
    record.aperture_section = pi.aperture_section;
    record.filter_id = pi.filter_id;
    record.filter_strength = pi.filter_strength;
    record.vignette_intensity = pi.vignette_intensity;
    record.light_id = pi.light_id;
    record.light_strength = pi.light_strength;
    record.nikki_loc_x = pi.nikki_loc_x;
    record.nikki_loc_y = pi.nikki_loc_y;
    record.nikki_loc_z = pi.nikki_loc_z;
    record.nikki_hidden = pi.nikki_hidden;
    record.pose_id = pi.pose_id;

    if (const auto& nikki_diy = pi.nikki_diy.value()) {
      record.nikki_diy_json = rfl::json::write(*nikki_diy);
    }
    if (const auto& nikki_clothes = pi.nikki_clothes.value()) {
      for (double value : *nikki_clothes) {
        record.nikki_clothes.push_back(static_cast<std::int64_t>(std::llround(value)));
      }
    }
  }

  return record;
}

auto http_post_json(Core::State::AppState& app_state, const std::string& url_utf8,
                    const std::string& request_body_utf8)
    -> asio::awaitable<std::expected<std::string, std::string>> {
  Core::HttpClient::Types::Request request{
      .method = "POST",
      .url = url_utf8,
      .headers =
          {
              Core::HttpClient::Types::Header{.name = "Content-Type", .value = "application/json"},
              Core::HttpClient::Types::Header{.name = "X-Client", .value = "SpinningMomo"},
          },
      .body = request_body_utf8,
  };

  auto response = co_await Core::HttpClient::fetch(app_state, request);
  if (!response) {
    co_return std::unexpected("Failed to send HTTP request: " + response.error());
  }

  if (response->status_code < 200 || response->status_code >= 300) {
    co_return std::unexpected("HTTP error: " + std::to_string(response->status_code));
  }

  co_return response->body;
}

auto parse_photo_params_records_from_response(const std::string& response_body)
    -> std::expected<std::vector<std::optional<ParsedPhotoParamsRecord>>, std::string> {
  auto response =
      rfl::json::read<ApiResponse, rfl::SnakeCaseToCamelCase, rfl::DefaultIfMissing>(response_body);
  if (!response) {
    return std::unexpected("Invalid JSON response: " + response.error().what());
  }
  const auto& parsed = response.value();

  if (!parsed.success) {
    std::string error = "API returned success=false";
    if (parsed.error && parsed.error->message) {
      error += ": " + *parsed.error->message;
    }
    return std::unexpected(error);
  }

  if (!parsed.data) {
    return std::unexpected("Response missing 'data' field");
  }

  std::vector<std::optional<ParsedPhotoParamsRecord>> records;
  records.reserve(parsed.data->size());
  for (const auto& item : *parsed.data) {
    auto social_photo = item ? item->social_photo.value() : std::nullopt;
    records.push_back(social_photo ? std::optional{to_parsed_record(*social_photo)} : std::nullopt);
  }
  return records;
}

template <typename T>
  requires std::same_as<T, std::string> || std::same_as<T, std::int64_t> ||
           std::same_as<T, double> || std::same_as<T, bool>
auto to_db_param(const std::optional<T>& value) -> Core::Database::Types::DbParam {
  if (!value) {
    return Core::Database::Types::DbParam{std::monostate{}};
  }

  if constexpr (std::same_as<T, bool>) {
    return Core::Database::Types::DbParam{static_cast<std::int64_t>(*value)};
  } else {
    return Core::Database::Types::DbParam{*value};
  }
}

auto load_candidate_assets(Core::State::AppState& app_state, bool only_missing)
    -> std::expected<std::vector<Scan::CandidateAssetRow>, std::string> {
  std::string sql = R"(
    SELECT a.id, a.path
    FROM assets a
    LEFT JOIN asset_infinity_nikki_params p ON p.asset_id = a.id
    WHERE a.type = 'photo'
      AND instr(replace(a.path, '\\', '/'), '/X6Game/Saved/GamePlayPhotos/') > 0
      AND instr(replace(a.path, '\\', '/'), '/NikkiPhotos_HighQuality/') > 0
      AND (lower(coalesce(a.extension, '')) IN ('.jpg', '.jpeg')
           OR lower(a.path) LIKE '%.jpg'
           OR lower(a.path) LIKE '%.jpeg')
  )";

  if (only_missing) {
    sql += " AND p.asset_id IS NULL";
  }

  sql += " ORDER BY COALESCE(a.file_modified_at, a.created_at) DESC, a.id DESC";

  auto query_result = Core::Database::query<Scan::CandidateAssetRow>(*app_state.database, sql, {});
  if (!query_result) {
    return std::unexpected("Failed to query candidate assets: " + query_result.error());
  }

  return query_result.value();
}

auto extract_batch_photo_params(Core::State::AppState& app_state,
                                const std::vector<Scan::PreparedPhotoExtractEntry>& entries)
    -> asio::awaitable<
        std::expected<std::vector<std::optional<ParsedPhotoParamsRecord>>, std::string>> {
  if (entries.empty()) {
    co_return std::vector<std::optional<ParsedPhotoParamsRecord>>{};
  }

  const auto& uid = entries.front().uid;
  for (const auto& entry : entries) {
    if (entry.uid != uid) {
      co_return std::unexpected("Batch contains multiple UIDs");
    }
  }

  ExtractApiRequestBody request_body{
      .uid = uid,
      .params_base64_list = {},
  };
  request_body.params_base64_list.reserve(entries.size());
  for (const auto& entry : entries) {
    request_body.params_base64_list.push_back(entry.params_base64);
  }

  auto request_json = rfl::json::write<rfl::SnakeCaseToCamelCase>(request_body);
  auto response_result =
      co_await http_post_json(app_state, std::string(kExtractApiUrl), request_json);
  if (!response_result) {
    co_return std::unexpected("HTTP error: " + response_result.error());
  }

  auto parsed_result = parse_photo_params_records_from_response(response_result.value());
  if (!parsed_result) {
    co_return std::unexpected(parsed_result.error());
  }

  auto records = std::move(parsed_result.value());
  if (records.size() != entries.size()) {
    co_return std::unexpected(
        std::format("response count mismatch: got {} for {}", records.size(), entries.size()));
  }

  co_return records;
}

auto upsert_photo_params_record(Core::State::AppState& app_state, std::int64_t asset_id,
                                const std::string& uid, const ParsedPhotoParamsRecord& record)
    -> std::expected<std::int32_t, std::string> {
  auto transaction_result = Core::Database::execute_transaction(
      *app_state.database, [&](auto& db_state) -> std::expected<std::int32_t, std::string> {
        std::string sql = R"(
          INSERT INTO asset_infinity_nikki_params (
            asset_id, uid, camera_params,
            time_day, time_hour, time_min, time_sec,
            camera_focal_length, aperture_section, filter_id, filter_strength, vignette_intensity,
            light_id, light_strength,
            nikki_loc_x, nikki_loc_y, nikki_loc_z,
            nikki_hidden, pose_id, nikki_diy_json
          ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
          ON CONFLICT(asset_id) DO UPDATE SET
            uid = excluded.uid,
            camera_params = excluded.camera_params,
            time_day = excluded.time_day,
            time_hour = excluded.time_hour,
            time_min = excluded.time_min,
            time_sec = excluded.time_sec,
            camera_focal_length = excluded.camera_focal_length,
            aperture_section = excluded.aperture_section,
            filter_id = excluded.filter_id,
            filter_strength = excluded.filter_strength,
            vignette_intensity = excluded.vignette_intensity,
            light_id = excluded.light_id,
            light_strength = excluded.light_strength,
            nikki_loc_x = excluded.nikki_loc_x,
            nikki_loc_y = excluded.nikki_loc_y,
            nikki_loc_z = excluded.nikki_loc_z,
            nikki_hidden = excluded.nikki_hidden,
            pose_id = excluded.pose_id,
            nikki_diy_json = excluded.nikki_diy_json
        )";

        std::vector<Core::Database::Types::DbParam> params = {
            asset_id,
            uid,
            to_db_param(record.camera_params),
            to_db_param(record.time_day),
            to_db_param(record.time_hour),
            to_db_param(record.time_min),
            to_db_param(record.time_sec),
            to_db_param(record.camera_focal_length),
            to_db_param(record.aperture_section),
            to_db_param(record.filter_id),
            to_db_param(record.filter_strength),
            to_db_param(record.vignette_intensity),
            to_db_param(record.light_id),
            to_db_param(record.light_strength),
            to_db_param(record.nikki_loc_x),
            to_db_param(record.nikki_loc_y),
            to_db_param(record.nikki_loc_z),
            to_db_param(record.nikki_hidden),
            to_db_param(record.pose_id),
            to_db_param(record.nikki_diy_json),
        };

        auto upsert_result = Core::Database::execute(db_state, sql, params);
        if (!upsert_result) {
          return std::unexpected("Failed to upsert Infinity Nikki params: " +
                                 upsert_result.error());
        }

        auto delete_clothes_result = Core::Database::execute(
            db_state, "DELETE FROM asset_infinity_nikki_clothes WHERE asset_id = ?", {asset_id});
        if (!delete_clothes_result) {
          return std::unexpected("Failed to clear existing clothes: " +
                                 delete_clothes_result.error());
        }

        std::int32_t inserted_clothes = 0;
        for (const auto cloth_id : record.nikki_clothes) {
          auto insert_cloth_result = Core::Database::execute(
              db_state,
              "INSERT INTO asset_infinity_nikki_clothes (asset_id, cloth_id) VALUES (?, ?)",
              {asset_id, cloth_id});
          if (!insert_cloth_result) {
            return std::unexpected("Failed to insert cloth mapping: " +
                                   insert_cloth_result.error());
          }
          inserted_clothes++;
        }

        return inserted_clothes;
      });

  return transaction_result;
}

}  // namespace Plugins::InfinityNikki::PhotoExtract::Infra
