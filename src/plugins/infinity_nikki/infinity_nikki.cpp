module;

#include <wil/com.h>

module Plugins.InfinityNikki;

import std;
import Core.Database;
import Core.Database.Types;
import Core.State;
import Plugins.InfinityNikki.Types;
import Utils.Logger;
import Utils.String;
import Vendor.WIL;
import Vendor.Windows;
import Vendor.ShellApi;
import Vendor.WinHttp;
import <rfl/json.hpp>;

namespace Plugins::InfinityNikki {

auto to_filesystem_path(const std::string& utf8_path) -> std::filesystem::path {
  return std::filesystem::path(Utils::String::FromUtf8(utf8_path));
}

auto has_valid_game_executable(const std::string& game_dir_utf8) -> bool {
  auto game_dir_path = to_filesystem_path(game_dir_utf8);
  if (game_dir_path.empty() || !std::filesystem::exists(game_dir_path) ||
      !std::filesystem::is_directory(game_dir_path)) {
    return false;
  }

  auto game_exe_path = game_dir_path / L"InfinityNikki.exe";
  return std::filesystem::exists(game_exe_path) && std::filesystem::is_regular_file(game_exe_path);
}

// 获取用户配置文件路径
auto get_config_file_path() -> std::filesystem::path {
  wil::unique_cotaskmem_string local_app_data_path;

  HRESULT hr = Vendor::ShellApi::SHGetKnownFolderPath(Vendor::ShellApi::kFOLDERID_LocalAppData, 0,
                                                      nullptr, &local_app_data_path);

  if (Vendor::Windows::_SUCCEEDED(hr) && local_app_data_path) {
    std::filesystem::path config_path = local_app_data_path.get();
    config_path /= L"InfinityNikki Launcher\\config.ini";
    return config_path;
  }

  return {};
}

auto get_game_directory_from_config(const std::filesystem::path& config_path)
    -> std::expected<std::string, std::string> {
  constexpr Vendor::Windows::DWORD buffer_size = Vendor::Windows::kMAX_PATH * 2;
  auto buffer = wil::make_unique_hlocal_nothrow<wchar_t[]>(buffer_size);
  if (!buffer) {
    return std::unexpected("Memory allocation failed");
  }

  Vendor::Windows::DWORD result = Vendor::Windows::GetPrivateProfileStringW(
      L"Download", L"gameDir", L"", buffer.get(), buffer_size, config_path.wstring().c_str());
  if (result == 0) {
    return std::unexpected("gameDir not found in config file");
  }

  return Utils::String::ToUtf8(buffer.get());
}

auto get_game_directory() -> std::expected<InfinityNikkiGameDirResult, std::string> {
  InfinityNikkiGameDirResult result;

  auto config_path = get_config_file_path();
  if (config_path.empty()) {
    result.message = "Failed to get user profile path";
    return result;
  }

  Logger().info("Checking config file at: {}", config_path.string());

  if (!std::filesystem::exists(config_path)) {
    result.message = "Config file not found at " + config_path.string();
    return result;
  }

  result.config_found = true;

  auto game_dir_result = get_game_directory_from_config(config_path);
  if (!game_dir_result) {
    result.message = game_dir_result.error();
    return result;
  }

  auto game_dir = game_dir_result.value();
  if (!has_valid_game_executable(game_dir)) {
    result.message = "Invalid game directory: InfinityNikki.exe not found";
    Logger().warn("Detected gameDir is invalid: {}", game_dir);
    return result;
  }

  result.game_dir = game_dir;
  result.game_dir_found = true;
  result.message = "Game directory found successfully";
  Logger().info("Found Infinity Nikki game directory: {}", *result.game_dir);

  return result;
}

struct CandidateAssetRow {
  std::int64_t id;
  std::string path;
};

struct PendingPhotoExtractEntry {
  std::int64_t asset_id;
  std::string uid;
  std::string params_base64;
};

struct ExtractApiRequestBody {
  std::string uid;
  std::vector<std::string> params_base64_list;
};

struct ParsedPhotoParamsRecord {
  std::optional<std::string> camera_params;
  std::optional<std::int64_t> time_day;
  std::optional<std::int64_t> time_hour;
  std::optional<std::int64_t> time_min;
  std::optional<double> time_sec;
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
  std::optional<std::string> nikki_diy_json;
  std::vector<std::int64_t> nikki_clothes;
};

constexpr std::string_view kExtractApiUrl = "http://api.infinitymomo.com/api/v1/extract";
constexpr std::size_t kMaxErrorMessages = 50;
constexpr std::size_t kExtractBatchSize = 32;

auto add_error(std::vector<std::string>& errors, std::string message) -> void {
  if (errors.size() >= kMaxErrorMessages) {
    return;
  }
  errors.push_back(std::move(message));
}

auto normalize_path_for_matching(std::string path) -> std::string {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

auto extract_uid_from_asset_path(const std::string& asset_path) -> std::optional<std::string> {
  auto normalized = normalize_path_for_matching(asset_path);
  constexpr std::string_view kPrefix = "/X6Game/Saved/GamePlayPhotos/";
  constexpr std::string_view kSuffix = "/NikkiPhotos_HighQuality/";

  auto prefix_pos = normalized.find(kPrefix);
  if (prefix_pos == std::string::npos) {
    return std::nullopt;
  }

  auto uid_start = prefix_pos + kPrefix.size();
  auto uid_end = normalized.find('/', uid_start);
  if (uid_end == std::string::npos || uid_end <= uid_start) {
    return std::nullopt;
  }

  auto suffix_pos = normalized.find(kSuffix, uid_end);
  if (suffix_pos == std::string::npos) {
    return std::nullopt;
  }

  return normalized.substr(uid_start, uid_end - uid_start);
}

auto is_base64_text(std::string_view text) -> bool {
  if (text.empty() || text.size() % 4 != 0) {
    return false;
  }

  for (unsigned char ch : text) {
    if (std::isalnum(ch) || ch == '+' || ch == '/' || ch == '=') {
      continue;
    }
    return false;
  }
  return true;
}

auto extract_params_base64_from_file(const std::filesystem::path& file_path)
    -> std::expected<std::string, std::string> {
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return std::unexpected("Failed to open photo file");
  }

  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
  if (data.size() < 6) {
    return std::unexpected("Photo file is too small");
  }

  std::vector<std::size_t> eoi_positions;
  eoi_positions.reserve(4);
  for (std::size_t i = 0; i + 1 < data.size(); ++i) {
    if (data[i] == 0xFF && data[i + 1] == 0xD9) {
      eoi_positions.push_back(i);
    }
  }

  if (eoi_positions.size() < 2) {
    return std::unexpected("Not a dual-EOI photo");
  }

  auto p1 = eoi_positions[eoi_positions.size() - 2];
  auto p2 = eoi_positions[eoi_positions.size() - 1];
  if (p2 <= p1 + 2) {
    return std::unexpected("Invalid EOI boundaries");
  }

  std::string middle(reinterpret_cast<const char*>(data.data() + p1 + 2), p2 - (p1 + 2));
  middle.erase(std::remove_if(middle.begin(), middle.end(),
                              [](unsigned char ch) { return std::isspace(ch) != 0; }),
               middle.end());

  if (!is_base64_text(middle)) {
    return std::unexpected("Middle segment is not valid Base64 text");
  }

  return middle;
}

auto http_post_json(const std::string& url_utf8, const std::string& request_body_utf8)
    -> std::expected<std::string, std::string> {
  auto url = Utils::String::FromUtf8(url_utf8);
  if (url.empty()) {
    return std::unexpected("Invalid URL encoding");
  }

  Vendor::WinHttp::UniqueHInternet h_session{Vendor::WinHttp::WinHttpOpen(
      L"SpinningMomo/1.0", Vendor::WinHttp::kWINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      Vendor::WinHttp::kWINHTTP_NO_PROXY_NAME, Vendor::WinHttp::kWINHTTP_NO_PROXY_BYPASS, 0)};
  if (!h_session) {
    return std::unexpected("Failed to open WinHTTP session");
  }

  Vendor::WinHttp::URL_COMPONENTS url_components = {sizeof(url_components)};
  wchar_t host_name[256] = {0};
  wchar_t url_path[2048] = {0};
  url_components.lpszHostName = host_name;
  url_components.dwHostNameLength = sizeof(host_name) / sizeof(wchar_t);
  url_components.lpszUrlPath = url_path;
  url_components.dwUrlPathLength = sizeof(url_path) / sizeof(wchar_t);

  if (!Vendor::WinHttp::WinHttpCrackUrl(
          url.c_str(), static_cast<Vendor::WinHttp::DWORD>(url.size()), 0, &url_components)) {
    return std::unexpected("Failed to parse URL");
  }

  Vendor::WinHttp::UniqueHInternet h_connect{
      Vendor::WinHttp::WinHttpConnect(h_session.get(), host_name, url_components.nPort, 0)};
  if (!h_connect) {
    return std::unexpected("Failed to connect to API host");
  }

  Vendor::WinHttp::DWORD flags = (url_components.nScheme == Vendor::WinHttp::kINTERNET_SCHEME_HTTPS)
                                     ? Vendor::WinHttp::kWINHTTP_FLAG_SECURE
                                     : 0;
  Vendor::WinHttp::UniqueHInternet h_request{Vendor::WinHttp::WinHttpOpenRequest(
      h_connect.get(), L"POST", url_path, nullptr, Vendor::WinHttp::kWINHTTP_NO_REFERER,
      Vendor::WinHttp::kWINHTTP_DEFAULT_ACCEPT_TYPES, flags)};
  if (!h_request) {
    return std::unexpected("Failed to open HTTP request");
  }

  std::wstring headers = L"Content-Type: application/json\r\nX-Client: SpinningMomo\r\n";
  auto body_size = static_cast<Vendor::WinHttp::DWORD>(request_body_utf8.size());
  void* request_data = body_size == 0 ? Vendor::WinHttp::kWINHTTP_NO_REQUEST_DATA
                                      : const_cast<char*>(request_body_utf8.data());

  if (!Vendor::WinHttp::WinHttpSendRequest(h_request.get(), headers.c_str(),
                                           static_cast<Vendor::WinHttp::DWORD>(headers.size()),
                                           request_data, body_size, body_size, 0)) {
    return std::unexpected("Failed to send HTTP request");
  }

  if (!Vendor::WinHttp::WinHttpReceiveResponse(h_request.get(), nullptr)) {
    return std::unexpected("Failed to receive HTTP response");
  }

  Vendor::WinHttp::DWORD status_code = 0;
  Vendor::WinHttp::DWORD status_size = sizeof(status_code);
  if (Vendor::WinHttp::WinHttpQueryHeaders(
          h_request.get(),
          Vendor::WinHttp::kWINHTTP_QUERY_STATUS_CODE | Vendor::WinHttp::kWINHTTP_QUERY_FLAG_NUMBER,
          Vendor::WinHttp::kWINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, nullptr)) {
    if (status_code < 200 || status_code >= 300) {
      return std::unexpected("HTTP error: " + std::to_string(status_code));
    }
  }

  std::string response;
  Vendor::WinHttp::DWORD available = 0;
  Vendor::WinHttp::DWORD downloaded = 0;
  char buffer[4096];

  while (true) {
    available = 0;
    if (!Vendor::WinHttp::WinHttpQueryDataAvailable(h_request.get(), &available)) {
      break;
    }
    if (available == 0) {
      break;
    }

    auto to_read = std::min(available, static_cast<Vendor::WinHttp::DWORD>(sizeof(buffer)));
    std::memset(buffer, 0, sizeof(buffer));
    if (!Vendor::WinHttp::WinHttpReadData(h_request.get(), buffer, to_read, &downloaded)) {
      break;
    }
    response.append(buffer, downloaded);
  }

  return response;
}

auto get_optional_object(const rfl::Generic::Object& object, const std::string& key)
    -> std::optional<rfl::Generic::Object> {
  auto value_result = object.get(key);
  if (!value_result) {
    return std::nullopt;
  }

  auto object_result = value_result.value().to_object();
  if (!object_result) {
    return std::nullopt;
  }

  return object_result.value();
}

auto get_optional_string(const rfl::Generic::Object& object, const std::string& key)
    -> std::optional<std::string> {
  auto result = object.get(key).and_then(rfl::to_string);
  if (result) return result.value();
  return std::nullopt;
}

auto get_optional_bool(const rfl::Generic::Object& object, const std::string& key)
    -> std::optional<bool> {
  auto result = object.get(key).and_then(rfl::to_bool);
  if (result) return result.value();
  return std::nullopt;
}

auto get_optional_int64(const rfl::Generic::Object& object, const std::string& key)
    -> std::optional<std::int64_t> {
  auto val = object.get(key);
  if (!val) return std::nullopt;
  if (auto r = rfl::to_int(val.value())) return static_cast<std::int64_t>(r.value());
  if (auto r = rfl::to_double(val.value()))
    return static_cast<std::int64_t>(std::llround(r.value()));
  return std::nullopt;
}

auto get_optional_double(const rfl::Generic::Object& object, const std::string& key)
    -> std::optional<double> {
  auto val = object.get(key);
  if (!val) return std::nullopt;
  if (auto r = rfl::to_double(val.value())) return r.value();
  if (auto r = rfl::to_int(val.value())) return static_cast<double>(r.value());
  return std::nullopt;
}

auto collect_payload_objects_with_social_photo(const rfl::Generic& node,
                                               std::vector<rfl::Generic::Object>& out,
                                               int depth = 0) -> void {
  if (depth > 6) {
    return;
  }

  if (auto object_result = node.to_object(); object_result) {
    const auto& object = object_result.value();
    if (auto social_photo = object.get("SocialPhoto").and_then(rfl::to_object); social_photo) {
      out.push_back(object);
    }

    for (const auto& key : {"data", "result", "results", "items", "payload"}) {
      auto child_result = object.get(key);
      if (!child_result) {
        continue;
      }
      collect_payload_objects_with_social_photo(child_result.value(), out, depth + 1);
    }
  }

  if (auto array_result = node.to_array(); array_result) {
    for (const auto& item : array_result.value()) {
      collect_payload_objects_with_social_photo(item, out, depth + 1);
    }
  }
}

auto parse_photo_params_record_from_payload(const rfl::Generic::Object& payload_object)
    -> std::expected<ParsedPhotoParamsRecord, std::string> {
  auto social_photo_result = payload_object.get("SocialPhoto").and_then(rfl::to_object);
  if (!social_photo_result) {
    return std::unexpected("Payload SocialPhoto is missing");
  }

  const auto& social_photo = social_photo_result.value();
  ParsedPhotoParamsRecord record;
  record.camera_params = get_optional_string(social_photo, "CameraParams");

  if (auto time_object = get_optional_object(social_photo, "Time"); time_object.has_value()) {
    record.time_day = get_optional_int64(*time_object, "day");
    record.time_hour = get_optional_int64(*time_object, "hour");
    record.time_min = get_optional_int64(*time_object, "min");
    record.time_sec = get_optional_double(*time_object, "sec");
  }

  if (auto photo_info = get_optional_object(social_photo, "PhotoInfo"); photo_info.has_value()) {
    record.camera_focal_length = get_optional_double(*photo_info, "cameraFocalLength");
    record.aperture_section = get_optional_int64(*photo_info, "apertureSection");
    record.filter_id = get_optional_string(*photo_info, "filterId");
    record.filter_strength = get_optional_double(*photo_info, "filterStrength");
    record.vignette_intensity = get_optional_double(*photo_info, "vignetteIntensity");
    record.light_id = get_optional_string(*photo_info, "lightId");
    record.light_strength = get_optional_double(*photo_info, "lightStrength");
    record.nikki_loc_x = get_optional_double(*photo_info, "nikkiLocX");
    record.nikki_loc_y = get_optional_double(*photo_info, "nikkiLocY");
    record.nikki_loc_z = get_optional_double(*photo_info, "nikkiLocZ");
    record.nikki_hidden = get_optional_bool(*photo_info, "nikkiHidden");
    record.pose_id = get_optional_int64(*photo_info, "poseId");

    auto nikki_diy_value = photo_info->get("nikkiDIY");
    if (nikki_diy_value) {
      record.nikki_diy_json = rfl::json::write(nikki_diy_value.value());
    }

    auto clothes_result = photo_info->get("nikkiClothes").and_then(rfl::to_array);
    if (clothes_result) {
      for (const auto& cloth_value : clothes_result.value()) {
        if (auto int_value = cloth_value.to_int(); int_value) {
          record.nikki_clothes.push_back(static_cast<std::int64_t>(int_value.value()));
          continue;
        }
        if (auto double_value = cloth_value.to_double(); double_value) {
          record.nikki_clothes.push_back(
              static_cast<std::int64_t>(std::llround(double_value.value())));
        }
      }
      std::ranges::sort(record.nikki_clothes);
      record.nikki_clothes.erase(
          std::unique(record.nikki_clothes.begin(), record.nikki_clothes.end()),
          record.nikki_clothes.end());
    }
  }

  return record;
}

auto parse_photo_params_records_from_response(const std::string& response_body)
    -> std::expected<std::vector<ParsedPhotoParamsRecord>, std::string> {
  auto parsed_result = rfl::json::read<rfl::Generic>(response_body);
  if (!parsed_result) {
    return std::unexpected("Invalid JSON response: " + parsed_result.error().what());
  }

  std::vector<rfl::Generic::Object> payload_objects;
  collect_payload_objects_with_social_photo(parsed_result.value(), payload_objects);
  if (payload_objects.empty()) {
    return std::unexpected("Response payload does not contain SocialPhoto");
  }

  std::vector<ParsedPhotoParamsRecord> records;
  records.reserve(payload_objects.size());
  for (std::size_t i = 0; i < payload_objects.size(); ++i) {
    auto record_result = parse_photo_params_record_from_payload(payload_objects[i]);
    if (!record_result) {
      return std::unexpected("Failed to parse payload item " + std::to_string(i) + ": " +
                             record_result.error());
    }
    records.push_back(std::move(record_result.value()));
  }

  return records;
}

template <typename T>
  requires std::same_as<T, std::string> || std::same_as<T, std::int64_t> ||
           std::same_as<T, double> || std::same_as<T, bool>
auto to_db_param(const std::optional<T>& value) -> Core::Database::Types::DbParam {
  if (!value) return Core::Database::Types::DbParam{std::monostate{}};
  if constexpr (std::same_as<T, bool>) {
    return Core::Database::Types::DbParam{static_cast<std::int64_t>(*value)};
  } else {
    return Core::Database::Types::DbParam{*value};
  }
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

auto load_candidate_assets(Core::State::AppState& app_state, bool only_missing)
    -> std::expected<std::vector<CandidateAssetRow>, std::string> {
  std::string sql = R"(
    SELECT a.id, a.path
    FROM assets a
    LEFT JOIN asset_infinity_nikki_params p ON p.asset_id = a.id
    WHERE a.type = 'photo'
      AND instr(replace(a.path, '\', '/'), '/X6Game/Saved/GamePlayPhotos/') > 0
      AND instr(replace(a.path, '\', '/'), '/NikkiPhotos_HighQuality/') > 0
      AND (lower(coalesce(a.extension, '')) IN ('.jpg', '.jpeg')
           OR lower(a.path) LIKE '%.jpg'
           OR lower(a.path) LIKE '%.jpeg')
  )";

  if (only_missing) {
    sql += " AND p.asset_id IS NULL";
  }

  sql += " ORDER BY COALESCE(a.file_modified_at, a.created_at) DESC, a.id DESC";

  auto query_result = Core::Database::query<CandidateAssetRow>(*app_state.database, sql, {});
  if (!query_result) {
    return std::unexpected("Failed to query candidate assets: " + query_result.error());
  }

  return query_result.value();
}

auto report_extract_progress(
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback,
    std::string stage, std::int64_t current, std::int64_t total,
    std::optional<std::string> message = std::nullopt) -> void {
  if (!progress_callback) {
    return;
  }

  std::optional<double> percent;
  if (total > 0) {
    percent =
        std::clamp((static_cast<double>(current) * 100.0) / static_cast<double>(total), 0.0, 100.0);
  }

  progress_callback(InfinityNikkiExtractPhotoParamsProgress{
      .stage = std::move(stage),
      .current = current,
      .total = total,
      .percent = percent,
      .message = std::move(message),
  });
}

auto extract_photo_params(
    Core::State::AppState& app_state, const InfinityNikkiExtractPhotoParamsRequest& request,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> std::expected<InfinityNikkiExtractPhotoParamsResult, std::string> {
  InfinityNikkiExtractPhotoParamsResult result;

  if (!app_state.database) {
    return std::unexpected("Database is not initialized");
  }

  auto only_missing = request.only_missing.value_or(true);

  auto candidates_result = load_candidate_assets(app_state, only_missing);
  if (!candidates_result) {
    return std::unexpected(candidates_result.error());
  }

  auto candidates = std::move(candidates_result.value());
  result.candidate_count = static_cast<std::int32_t>(candidates.size());

  if (candidates.empty()) {
    report_extract_progress(progress_callback, "completed", 0, 0, "No candidate assets");
    return result;
  }

  report_extract_progress(progress_callback, "processing", 0, result.candidate_count,
                          std::format("Processing {} candidate photos", result.candidate_count));

  std::vector<PendingPhotoExtractEntry> pending_entries;
  pending_entries.reserve(candidates.size());

  auto mark_progress = [&](std::optional<std::string> message = std::nullopt) {
    report_extract_progress(progress_callback, "processing", result.processed_count,
                            result.candidate_count, std::move(message));
  };

  for (const auto& candidate : candidates) {
    auto uid_result = extract_uid_from_asset_path(candidate.path);
    if (!uid_result.has_value()) {
      result.skipped_count++;
      add_error(result.errors,
                std::format("asset_id {} skipped: cannot parse UID from path", candidate.id));
      result.processed_count++;
      mark_progress();
      continue;
    }

    auto photo_path = to_filesystem_path(candidate.path);
    if (!std::filesystem::exists(photo_path)) {
      result.skipped_count++;
      add_error(result.errors,
                std::format("asset_id {} skipped: photo file does not exist", candidate.id));
      result.processed_count++;
      mark_progress();
      continue;
    }

    auto params_base64_result = extract_params_base64_from_file(photo_path);
    if (!params_base64_result) {
      result.skipped_count++;
      add_error(result.errors,
                std::format("asset_id {} skipped: {}", candidate.id, params_base64_result.error()));
      result.processed_count++;
      mark_progress();
      continue;
    }

    pending_entries.push_back(PendingPhotoExtractEntry{
        .asset_id = candidate.id,
        .uid = uid_result.value(),
        .params_base64 = params_base64_result.value(),
    });
  }

  std::unordered_map<std::string, std::vector<PendingPhotoExtractEntry>> groups_by_uid;
  groups_by_uid.reserve(pending_entries.size());
  for (auto& entry : pending_entries) {
    groups_by_uid[entry.uid].push_back(std::move(entry));
  }

  auto fail_batch = [&](const std::vector<PendingPhotoExtractEntry>& entries, std::size_t start,
                        std::size_t end, const std::string& reason) {
    for (std::size_t i = start; i < end; ++i) {
      result.failed_count++;
      result.processed_count++;
      add_error(result.errors, std::format("asset_id {} failed: {}", entries[i].asset_id, reason));
      mark_progress();
    }
  };

  for (auto& [uid, entries] : groups_by_uid) {
    for (std::size_t start = 0; start < entries.size(); start += kExtractBatchSize) {
      auto end = std::min(start + kExtractBatchSize, entries.size());
      auto batch_size = end - start;

      ExtractApiRequestBody request_body;
      request_body.uid = uid;
      request_body.params_base64_list.reserve(batch_size);
      for (std::size_t i = start; i < end; ++i) {
        request_body.params_base64_list.push_back(entries[i].params_base64);
      }

      auto request_json = rfl::json::write<rfl::SnakeCaseToCamelCase>(request_body);
      auto response_result = http_post_json(std::string(kExtractApiUrl), request_json);
      if (!response_result) {
        fail_batch(entries, start, end, response_result.error());
        continue;
      }

      auto parsed_result = parse_photo_params_records_from_response(response_result.value());
      if (!parsed_result) {
        fail_batch(entries, start, end, parsed_result.error());
        continue;
      }

      auto records = std::move(parsed_result.value());
      if (records.size() != batch_size) {
        fail_batch(entries, start, end, "response count mismatch");
        continue;
      }

      for (std::size_t i = 0; i < batch_size; ++i) {
        const auto& entry = entries[start + i];
        auto save_result = upsert_photo_params_record(app_state, entry.asset_id, uid, records[i]);
        if (!save_result) {
          result.failed_count++;
          add_error(result.errors,
                    std::format("asset_id {} failed: {}", entry.asset_id, save_result.error()));
        } else {
          result.saved_count++;
          result.clothes_rows_written += save_result.value();
        }

        result.processed_count++;
        mark_progress();
      }
    }
  }

  report_extract_progress(progress_callback, "completed", result.processed_count,
                          result.candidate_count,
                          std::format("Done: saved={}, skipped={}, failed={}", result.saved_count,
                                      result.skipped_count, result.failed_count));

  return result;
}

}  // namespace Plugins::InfinityNikki
