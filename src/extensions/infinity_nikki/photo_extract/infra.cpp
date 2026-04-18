module;

#include <asio.hpp>

module Extensions.InfinityNikki.PhotoExtract.Infra;

import std;
import Core.Database;
import Core.Database.Types;
import Core.State;
import Core.HttpClient;
import Core.HttpClient.Types;
import Features.Gallery.Folder.Repository;
import Extensions.InfinityNikki.PhotoExtract.Scan;
import Extensions.InfinityNikki.Types;
import <rfl/json.hpp>;

namespace Extensions::InfinityNikki::PhotoExtract::Infra {

struct ExtractApiRequestBody {
  std::string uid;
  std::vector<std::array<std::string, 2>> photos;
  std::optional<bool> raw_data = true;
  std::optional<bool> raw_id = true;
};

struct Nuan5Light {
  std::optional<std::int64_t> l;
  std::optional<double> s;
};

struct Nuan5RawTime {
  std::optional<std::int64_t> d;
  std::optional<std::int64_t> h;
  std::optional<std::int64_t> m;
  std::optional<double> s;
};

struct Nuan5RawPos {
  std::optional<double> x;
  std::optional<double> y;
  std::optional<double> z;
};

struct Nuan5RawData {
  std::optional<Nuan5RawTime> time;
  std::optional<Nuan5RawPos> pos;
};

struct Nuan5Filter {
  std::optional<std::int64_t> f;
  std::optional<double> s;
};

struct Nuan5DiyEntry {
  std::optional<std::int64_t> i;
  std::optional<std::string> t;
  std::optional<std::int64_t> grid;
};

struct Nuan5DecodedPhoto {
  std::optional<double> focal;
  std::optional<double> apeture;
  std::optional<double> rotation;
  std::optional<double> vignette;
  std::optional<Nuan5Filter> filter;
  std::optional<std::int64_t> pose;
  std::optional<Nuan5Light> light;
  std::optional<bool> hide_nikki;
  std::optional<std::string> camera_params;
  std::optional<bool> vertical;
  std::optional<double> bloomIntensity;
  std::optional<double> bloomThreshold;
  std::optional<double> brightness;
  std::optional<double> exposure;
  std::optional<double> contrast;
  std::optional<double> saturation;
  std::optional<double> vibrance;
  std::optional<double> highlights;
  std::optional<double> shadow;
  std::optional<std::vector<std::int64_t>> clothes;
  std::optional<std::unordered_map<std::string, std::vector<Nuan5DiyEntry>>> diy;
  std::optional<Nuan5RawData> raw;
};

constexpr std::string_view kExtractApiUrl = "https://nuan5.pro/api/decode-photo";
constexpr auto kNuan5MinRequestInterval = std::chrono::milliseconds(500);
constexpr std::size_t kMaxNuan5RateLimitRetries = 2;

struct HttpJsonResponse {
  int status_code = 0;
  std::string body;
};

auto to_parsed_record(const Nuan5DecodedPhoto& photo) -> ParsedPhotoParamsRecord {
  ParsedPhotoParamsRecord record;
  record.camera_params = photo.camera_params;
  record.camera_focal_length = photo.focal;
  record.rotation = photo.rotation;
  record.aperture_value = photo.apeture;
  if (photo.filter.has_value()) {
    record.filter_id = photo.filter->f;
    record.filter_strength = photo.filter->s;
  }
  record.vignette_intensity = photo.vignette;
  record.vertical = photo.vertical;
  record.bloom_intensity = photo.bloomIntensity;
  record.bloom_threshold = photo.bloomThreshold;
  record.brightness = photo.brightness;
  record.exposure = photo.exposure;
  record.contrast = photo.contrast;
  record.saturation = photo.saturation;
  record.vibrance = photo.vibrance;
  record.highlights = photo.highlights;
  record.shadow = photo.shadow;
  record.nikki_hidden = photo.hide_nikki;
  record.pose_id = photo.pose;

  if (photo.light.has_value()) {
    record.light_id = photo.light->l;
    record.light_strength = photo.light->s;
  }

  if (photo.raw.has_value()) {
    if (photo.raw->time.has_value()) {
      record.time_hour = photo.raw->time->h;
      record.time_min = photo.raw->time->m;
    }
    if (photo.raw->pos.has_value()) {
      record.nikki_loc_x = photo.raw->pos->x;
      record.nikki_loc_y = photo.raw->pos->y;
      record.nikki_loc_z = photo.raw->pos->z;
    }
  }
  if (photo.clothes.has_value()) {
    record.nikki_clothes = *photo.clothes;
  }
  if (photo.diy.has_value()) {
    record.nikki_diy_json = rfl::json::write(*photo.diy);
  }

  return record;
}

auto http_post_json(Core::State::AppState& app_state, const std::string& url_utf8,
                    const std::string& request_body_utf8)
    -> asio::awaitable<std::expected<HttpJsonResponse, std::string>> {
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

  co_return HttpJsonResponse{
      .status_code = response->status_code,
      .body = std::move(response->body),
  };
}

auto acquire_nuan5_send_slot() -> asio::awaitable<void> {
  static std::mutex gate_mutex;
  static auto next_allowed_at = std::chrono::steady_clock::time_point::min();

  auto executor = co_await asio::this_coro::executor;
  asio::steady_timer timer(executor);

  while (true) {
    auto wait_duration = std::chrono::steady_clock::duration::zero();
    {
      std::lock_guard<std::mutex> lock(gate_mutex);
      auto now = std::chrono::steady_clock::now();
      if (now >= next_allowed_at) {
        next_allowed_at = now + kNuan5MinRequestInterval;
        co_return;
      }
      wait_duration = next_allowed_at - now;
    }

    timer.expires_after(wait_duration);
    std::error_code wait_error;
    co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, wait_error));
  }
}

auto parse_photo_params_records_from_response(const std::string& response_body)
    -> std::expected<std::vector<ExtractBatchPhotoParamsRecord>, std::string> {
  auto response = rfl::json::read<std::vector<std::variant<std::string, Nuan5DecodedPhoto>>,
                                  rfl::DefaultIfMissing>(response_body);

  if (!response) {
    return std::unexpected("Invalid JSON response: " + response.error().what());
  }
  std::vector<ExtractBatchPhotoParamsRecord> records;
  records.reserve(response->size());
  for (const auto& item : *response) {
    if (std::holds_alternative<std::string>(item)) {
      records.push_back(ExtractBatchPhotoParamsRecord{
          .record = std::nullopt,
          .error_message = std::get<std::string>(item),
      });
      continue;
    }
    records.push_back(ExtractBatchPhotoParamsRecord{
        .record = to_parsed_record(std::get<Nuan5DecodedPhoto>(item)),
        .error_message = std::nullopt,
    });
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

auto normalize_path_for_like_match(std::string path) -> std::string {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

auto load_candidate_assets(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::vector<Scan::CandidateAssetRow>, std::string> {
  auto only_missing = request.only_missing.value_or(true);

  if (request.folder_id.has_value()) {
    auto folder_result =
        Features::Gallery::Folder::Repository::get_folder_by_id(app_state, *request.folder_id);
    if (!folder_result) {
      return std::unexpected("Failed to query folder for manual extract: " + folder_result.error());
    }
    if (!folder_result->has_value()) {
      return std::unexpected("Folder not found for manual extract: " +
                             std::to_string(*request.folder_id));
    }

    auto normalized_folder_path = normalize_path_for_like_match(folder_result->value().path);
    std::string sql = R"(
      SELECT a.id, a.path
      FROM assets a
      LEFT JOIN asset_infinity_nikki_params p ON p.asset_id = a.id
      WHERE a.type = 'photo'
        AND (
          replace(a.path, '\\', '/') = ?
          OR replace(a.path, '\\', '/') LIKE ?
        )
        AND (lower(coalesce(a.extension, '')) IN ('.jpg', '.jpeg')
             OR lower(a.path) LIKE '%.jpg'
             OR lower(a.path) LIKE '%.jpeg')
    )";

    std::vector<Core::Database::Types::DbParam> params = {
        normalized_folder_path,
        normalized_folder_path + "/%",
    };

    if (only_missing) {
      sql += " AND p.asset_id IS NULL";
    }

    sql += " ORDER BY COALESCE(a.file_modified_at, a.created_at) DESC, a.id DESC";

    auto query_result =
        Core::Database::query<Scan::CandidateAssetRow>(*app_state.database, sql, params);
    if (!query_result) {
      return std::unexpected("Failed to query manual extract candidate assets: " +
                             query_result.error());
    }

    return query_result.value();
  }

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
    -> asio::awaitable<std::expected<std::vector<ExtractBatchPhotoParamsRecord>, std::string>> {
  if (entries.empty()) {
    co_return std::vector<ExtractBatchPhotoParamsRecord>{};
  }

  const auto& uid = entries.front().uid;
  for (const auto& entry : entries) {
    if (entry.uid != uid) {
      co_return std::unexpected("Batch contains multiple UIDs");
    }
  }

  // 远端接口按“同一个 UID 的多张照片”来解析，
  // 所以这里明确要求一个 batch 内只能有一个 UID。
  ExtractApiRequestBody request_body{
      .uid = uid,
      .photos = {},
  };
  request_body.photos.reserve(entries.size());
  for (const auto& entry : entries) {
    request_body.photos.push_back({entry.md5, entry.encoded});
  }

  auto request_json = rfl::json::write(request_body);
  std::expected<std::string, std::string> response_body =
      std::unexpected("HTTP response is unavailable");
  for (std::size_t attempt = 0; attempt <= kMaxNuan5RateLimitRetries; ++attempt) {
    co_await acquire_nuan5_send_slot();

    auto response_result =
        co_await http_post_json(app_state, std::string(kExtractApiUrl), request_json);
    if (!response_result) {
      co_return std::unexpected("HTTP error: " + response_result.error());
    }

    if (response_result->status_code == 429) {
      if (attempt >= kMaxNuan5RateLimitRetries) {
        co_return std::unexpected(
            std::format("HTTP error: 429 (rate limit), retries exhausted after {}", attempt));
      }
      continue;
    }

    if (response_result->status_code < 200 || response_result->status_code >= 300) {
      co_return std::unexpected("HTTP error: " + std::to_string(response_result->status_code));
    }

    response_body = std::move(response_result->body);
    break;
  }

  if (!response_body) {
    co_return std::unexpected(response_body.error());
  }

  auto parsed_result = parse_photo_params_records_from_response(response_body.value());
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

auto upsert_photo_params_batch(Core::State::AppState& app_state, const std::string& uid,
                               const std::vector<ParsedPhotoParamsBatchItem>& items)
    -> std::expected<std::int32_t, std::string> {
  if (items.empty()) {
    return 0;
  }

  auto transaction_result = Core::Database::execute_transaction(
      *app_state.database, [&](auto& db_state) -> std::expected<std::int32_t, std::string> {
        // 这里故意使用“整个 batch 一个事务”。
        // 目标不是减少 SQL 条数到极致，而是先把最贵的事务提交次数降下来。
        std::string upsert_sql = R"(
          INSERT INTO asset_infinity_nikki_params (
            asset_id, uid, camera_params,
            time_hour, time_min,
            camera_focal_length, rotation, aperture_value,
            filter_id, filter_strength, vignette_intensity,
            light_id, light_strength,
            vertical, bloom_intensity, bloom_threshold, brightness, exposure, contrast, saturation,
            vibrance, highlights, shadow,
            nikki_loc_x, nikki_loc_y, nikki_loc_z,
            nikki_hidden, pose_id, nikki_diy_json
          ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
          ON CONFLICT(asset_id) DO UPDATE SET
            uid = excluded.uid,
            camera_params = excluded.camera_params,
            time_hour = excluded.time_hour,
            time_min = excluded.time_min,
            camera_focal_length = excluded.camera_focal_length,
            rotation = excluded.rotation,
            aperture_value = excluded.aperture_value,
            filter_id = excluded.filter_id,
            filter_strength = excluded.filter_strength,
            vignette_intensity = excluded.vignette_intensity,
            light_id = excluded.light_id,
            light_strength = excluded.light_strength,
            vertical = excluded.vertical,
            bloom_intensity = excluded.bloom_intensity,
            bloom_threshold = excluded.bloom_threshold,
            brightness = excluded.brightness,
            exposure = excluded.exposure,
            contrast = excluded.contrast,
            saturation = excluded.saturation,
            vibrance = excluded.vibrance,
            highlights = excluded.highlights,
            shadow = excluded.shadow,
            nikki_loc_x = excluded.nikki_loc_x,
            nikki_loc_y = excluded.nikki_loc_y,
            nikki_loc_z = excluded.nikki_loc_z,
            nikki_hidden = excluded.nikki_hidden,
            pose_id = excluded.pose_id,
            nikki_diy_json = excluded.nikki_diy_json
        )";

        std::int32_t inserted_clothes = 0;
        for (const auto& item : items) {
          // 每张照片仍然各自 upsert / clear clothes / insert clothes，
          // 但它们现在被包在同一个事务里，整体成本会低很多。
          const auto& record = item.record;

          std::vector<Core::Database::Types::DbParam> params = {
              item.asset_id,
              uid,
              to_db_param(record.camera_params),
              to_db_param(record.time_hour),
              to_db_param(record.time_min),
              to_db_param(record.camera_focal_length),
              to_db_param(record.rotation),
              to_db_param(record.aperture_value),
              to_db_param(record.filter_id),
              to_db_param(record.filter_strength),
              to_db_param(record.vignette_intensity),
              to_db_param(record.light_id),
              to_db_param(record.light_strength),
              to_db_param(record.vertical),
              to_db_param(record.bloom_intensity),
              to_db_param(record.bloom_threshold),
              to_db_param(record.brightness),
              to_db_param(record.exposure),
              to_db_param(record.contrast),
              to_db_param(record.saturation),
              to_db_param(record.vibrance),
              to_db_param(record.highlights),
              to_db_param(record.shadow),
              to_db_param(record.nikki_loc_x),
              to_db_param(record.nikki_loc_y),
              to_db_param(record.nikki_loc_z),
              to_db_param(record.nikki_hidden),
              to_db_param(record.pose_id),
              to_db_param(record.nikki_diy_json),
          };

          auto upsert_result = Core::Database::execute(db_state, upsert_sql, params);
          if (!upsert_result) {
            return std::unexpected("Failed to upsert Infinity Nikki params: " +
                                   upsert_result.error());
          }

          auto insert_cloth_result = Core::Database::execute(
              db_state, "DELETE FROM asset_infinity_nikki_clothes WHERE asset_id = ?",
              {item.asset_id});
          if (!insert_cloth_result) {
            return std::unexpected("Failed to clear existing clothes: " +
                                   insert_cloth_result.error());
          }

          for (const auto cloth_id : record.nikki_clothes) {
            auto insert_cloth_result = Core::Database::execute(
                db_state,
                "INSERT INTO asset_infinity_nikki_clothes (asset_id, cloth_id) VALUES (?, ?)",
                {item.asset_id, cloth_id});
            if (!insert_cloth_result) {
              return std::unexpected("Failed to insert cloth mapping: " +
                                     insert_cloth_result.error());
            }
            inserted_clothes++;
          }
        }

        return inserted_clothes;
      });

  return transaction_result;
}

}  // namespace Extensions::InfinityNikki::PhotoExtract::Infra
