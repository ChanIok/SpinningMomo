module;

#include <wil/com.h>
#include <rfl/json.hpp>

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
// import <rfl/json.hpp>;

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

// 查询阶段：从数据库取出的候选资产行
struct CandidateAssetRow {
  std::int64_t id;
  std::string path;
};

// 扫描阶段：从照片文件提取出的待发送条目
struct PreparedPhotoExtractEntry {
  std::int64_t asset_id;
  std::string uid;  // 用户 UID，取自路径中 GamePlayPhotos/<uid>/
  std::string params_base64;
};

// 解析 API 请求体：同一批次的所有照片必须属于同一个 UID
struct ExtractApiRequestBody {
  std::string uid;
  std::vector<std::string> params_base64_list;
};

// 以下三个结构体镜像 API 响应中 SocialPhoto 的 JSON schema，供 rfl 直接反序列化。
// SocialPhoto 级别字段为 PascalCase，PhotoInfo 级别字段为 camelCase（由 SnakeCaseToCamelCase
// 处理）， Time 级别字段为纯小写单词（单词无需转换）。
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
  // DIY 大写缩写和 Clothes 需显式 Rename；Clothes 元素统一用 double 接收，写入 DB 时再截断为整数
  rfl::Rename<"nikkiDIY", std::optional<rfl::Generic>> nikki_diy;
  rfl::Rename<"nikkiClothes", std::optional<std::vector<double>>> nikki_clothes;
};

struct SocialPhotoData {
  rfl::Rename<"CameraParams", std::optional<std::string>> camera_params;
  rfl::Rename<"Time", std::optional<PhotoTime>> time;
  rfl::Rename<"PhotoInfo", std::optional<PhotoInfo>> photo_info;
};

// 镜像 API 响应的顶层结构：
//   { "success": bool, "data": [ { "SocialPhoto": {...} } | null, ... ], "error": {...} }
// data 数组的每一项是 { "SocialPhoto": SocialPhotoData } 包装对象，或 null。
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

// 从 SocialPhotoData 映射到扁平的 DB 写入结构（展开嵌套层次，转换特殊字段）
// rfl::Rename<"Key", T> 字段需通过 .value() 解包出底层类型 T
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
      for (double v : *nikki_clothes) {
        record.nikki_clothes.push_back(static_cast<std::int64_t>(std::llround(v)));
      }
    }
  }

  return record;
}

constexpr std::string_view kExtractApiUrl = "https://api.infinitymomo.com/api/v1/extract";
constexpr std::size_t kMaxErrorMessages = 50;
constexpr std::size_t kExtractBatchSize = 32;
constexpr std::int64_t kMinProgressReportIntervalMillis = 200;
constexpr double kPreparingPercent = 2.0;
constexpr double kProcessingStartPercent = 5.0;
constexpr double kProcessingEndPercent = 99.0;

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

  auto prefix_pos = normalized.find(kPrefix);
  if (prefix_pos == std::string::npos) {
    return std::nullopt;
  }

  auto uid_start = prefix_pos + kPrefix.size();
  auto uid_end = normalized.find('/', uid_start);
  if (uid_end == std::string::npos || uid_end <= uid_start) {
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

// 从游戏照片文件中提取 Base64 编码的拍摄参数。
// 游戏的照片格式：在 JPEG 主图像的 EOI (0xFF 0xD9) 之后，紧接着追加了一段
// Base64 文本，再以第二个 EOI 结尾。本函数定位最后两个 EOI，取其间的内容。
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

  // 取倒数第二和最后一个 EOI，兼容 JPEG 内部可能出现的多余 EOI
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

// 解析 API 响应：{ "success": bool, "data": [ item | null, ... ] }
// data 数组与请求的 paramsBase64List 一一对应；null 表示该项无法解析，返回 nullopt。
// rfl 在单次反序列化中完成所有工作：字段名映射、null 处理、嵌套类型转换。
auto parse_photo_params_records_from_response(const std::string& response_body)
    -> std::expected<std::vector<std::optional<ParsedPhotoParamsRecord>>, std::string> {
  auto resp_result =
      rfl::json::read<ApiResponse, rfl::SnakeCaseToCamelCase, rfl::DefaultIfMissing>(response_body);
  if (!resp_result) {
    return std::unexpected("Invalid JSON response: " + resp_result.error().what());
  }
  const auto& resp = resp_result.value();

  if (!resp.success) {
    std::string err = "API returned success=false";
    if (resp.error && resp.error->message) {
      err += ": " + *resp.error->message;
    }
    return std::unexpected(err);
  }

  if (!resp.data) {
    return std::unexpected("Response missing 'data' field");
  }

  std::vector<std::optional<ParsedPhotoParamsRecord>> records;
  records.reserve(resp.data->size());
  for (const auto& item : *resp.data) {
    // item 是 optional<SocialPhotoWrapper>；其内部 social_photo 也是 optional
    auto sp = item ? item->social_photo.value() : std::nullopt;
    records.push_back(sp ? std::optional{to_parsed_record(*sp)} : std::nullopt);
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

struct ExtractProgressState {
  std::int64_t total_candidates = 0;
  std::int64_t scanned_count = 0;
  std::int64_t finalized_count = 0;
  int last_reported_percent = -1;
  std::int64_t last_report_millis = 0;
};

auto steady_clock_millis() -> std::int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

auto report_extract_progress(
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback,
    std::string stage, std::int64_t current, std::int64_t total,
    std::optional<double> percent = std::nullopt, std::optional<std::string> message = std::nullopt)
    -> void {
  if (!progress_callback) {
    return;
  }

  if (!percent.has_value() && total > 0) {
    percent = (static_cast<double>(current) * 100.0) / static_cast<double>(total);
  }

  if (percent.has_value()) {
    percent = std::clamp(*percent, 0.0, 100.0);
  }

  progress_callback(InfinityNikkiExtractPhotoParamsProgress{
      .stage = std::move(stage),
      .current = current,
      .total = total,
      .percent = percent,
      .message = std::move(message),
  });
}

// 进度百分比综合了"扫描"和"已落库"两个子阶段，各占一半权重，
// 使进度条在文件扫描期和 API 等待期都能持续推进，避免长时间停滞。
auto calculate_processing_percent(const ExtractProgressState& progress) -> double {
  if (progress.total_candidates <= 0) {
    return 100.0;
  }

  auto total = static_cast<double>(progress.total_candidates);
  auto scanned_ratio = std::clamp(static_cast<double>(progress.scanned_count) / total, 0.0, 1.0);
  auto finalized_ratio =
      std::clamp(static_cast<double>(progress.finalized_count) / total, 0.0, 1.0);
  auto overall_ratio = (scanned_ratio + finalized_ratio) / 2.0;

  return kProcessingStartPercent +
         overall_ratio * (kProcessingEndPercent - kProcessingStartPercent);
}

auto build_processing_message(const ExtractProgressState& progress,
                              const InfinityNikkiExtractPhotoParamsResult& result) -> std::string {
  return std::format("Scanned {} / {}, finalized {} / {}, saved {}, skipped {}, failed {}",
                     progress.scanned_count, progress.total_candidates, progress.finalized_count,
                     progress.total_candidates, result.saved_count, result.skipped_count,
                     result.failed_count);
}

auto report_processing_progress(
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback,
    ExtractProgressState& progress, const InfinityNikkiExtractPhotoParamsResult& result,
    bool force = false, std::optional<std::string> message = std::nullopt) -> void {
  if (!progress_callback || progress.total_candidates <= 0) {
    return;
  }

  auto percent = calculate_processing_percent(progress);
  auto rounded_percent = static_cast<int>(std::floor(percent));
  auto now = steady_clock_millis();

  if (!force) {
    if (rounded_percent <= progress.last_reported_percent) {
      return;
    }

    if (now - progress.last_report_millis < kMinProgressReportIntervalMillis) {
      return;
    }
  }

  progress.last_reported_percent = std::max(progress.last_reported_percent, rounded_percent);
  progress.last_report_millis = now;

  if (!message.has_value()) {
    message = build_processing_message(progress, result);
  }

  report_extract_progress(progress_callback, "processing", progress.finalized_count,
                          progress.total_candidates, percent, std::move(message));
}

auto prepare_photo_extract_entry(const CandidateAssetRow& candidate)
    -> std::expected<PreparedPhotoExtractEntry, std::string> {
  auto uid_result = extract_uid_from_asset_path(candidate.path);
  if (!uid_result.has_value()) {
    return std::unexpected("cannot parse UID from path");
  }

  auto params_base64_result = extract_params_base64_from_file(to_filesystem_path(candidate.path));
  if (!params_base64_result) {
    return std::unexpected(params_base64_result.error());
  }

  return PreparedPhotoExtractEntry{
      .asset_id = candidate.id,
      .uid = std::move(uid_result.value()),
      .params_base64 = std::move(params_base64_result.value()),
  };
}

auto mark_candidate_skipped(InfinityNikkiExtractPhotoParamsResult& result,
                            ExtractProgressState& progress, std::int64_t asset_id,
                            const std::string& reason) -> void {
  result.skipped_count++;
  result.processed_count++;
  progress.finalized_count++;
  add_error(result.errors, std::format("asset_id {} skipped: {}", asset_id, reason));
}

auto mark_candidate_failed(InfinityNikkiExtractPhotoParamsResult& result,
                           ExtractProgressState& progress, std::int64_t asset_id,
                           const std::string& reason) -> void {
  result.failed_count++;
  result.processed_count++;
  progress.finalized_count++;
  add_error(result.errors, std::format("asset_id {} failed: {}", asset_id, reason));
}

auto mark_candidate_saved(InfinityNikkiExtractPhotoParamsResult& result,
                          ExtractProgressState& progress, std::int32_t clothes_rows_written)
    -> void {
  result.saved_count++;
  result.processed_count++;
  result.clothes_rows_written += clothes_rows_written;
  progress.finalized_count++;
}

auto flush_extract_batch(
    Core::State::AppState& app_state, const std::vector<PreparedPhotoExtractEntry>& entries,
    InfinityNikkiExtractPhotoParamsResult& result, ExtractProgressState& progress,
    const std::function<void(const InfinityNikkiExtractPhotoParamsProgress&)>& progress_callback)
    -> void {
  if (entries.empty()) {
    return;
  }

  auto fail_all = [&](const std::string& reason) {
    Logger().warn("flush_extract_batch: batch failed (uid={}, count={}): {}", entries.front().uid,
                  entries.size(), reason);
    for (const auto& entry : entries) {
      mark_candidate_failed(result, progress, entry.asset_id, reason);
    }
    report_processing_progress(progress_callback, progress, result, true);
  };

  const auto& uid = entries.front().uid;

  Logger().debug("flush_extract_batch: sending batch (uid={}, count={})", uid, entries.size());

  ExtractApiRequestBody request_body;
  request_body.uid = uid;
  request_body.params_base64_list.reserve(entries.size());
  for (const auto& entry : entries) {
    request_body.params_base64_list.push_back(entry.params_base64);
  }

  auto request_json = rfl::json::write<rfl::SnakeCaseToCamelCase>(request_body);
  auto response_result = http_post_json(std::string(kExtractApiUrl), request_json);
  if (!response_result) {
    Logger().error("flush_extract_batch: HTTP request failed: {}", response_result.error());
    fail_all("HTTP error: " + response_result.error());
    return;
  }

  auto parsed_result = parse_photo_params_records_from_response(response_result.value());
  if (!parsed_result) {
    Logger().error("flush_extract_batch: parse response failed: {}", parsed_result.error());
    fail_all(parsed_result.error());
    return;
  }

  auto records = std::move(parsed_result.value());
  if (records.size() != entries.size()) {
    Logger().error("flush_extract_batch: response count mismatch: got {} records for {} entries",
                   records.size(), entries.size());
    fail_all(std::format("response count mismatch: got {} for {}", records.size(), entries.size()));
    return;
  }

  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    if (!records[i].has_value()) {
      mark_candidate_failed(result, progress, entry.asset_id,
                            "API returned null (photo params unrecognized)");
      continue;
    }
    auto save_result = upsert_photo_params_record(app_state, entry.asset_id, uid, *records[i]);
    if (!save_result) {
      Logger().error("flush_extract_batch: DB upsert failed for asset_id={}: {}", entry.asset_id,
                     save_result.error());
      mark_candidate_failed(result, progress, entry.asset_id, save_result.error());
      continue;
    }

    mark_candidate_saved(result, progress, save_result.value());
  }

  report_processing_progress(progress_callback, progress, result, true);
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

  report_extract_progress(progress_callback, "preparing", 0, 0, kPreparingPercent,
                          "Loading candidate assets");

  auto candidates_result = load_candidate_assets(app_state, only_missing);
  if (!candidates_result) {
    return std::unexpected(candidates_result.error());
  }

  auto candidates = std::move(candidates_result.value());
  result.candidate_count = static_cast<std::int32_t>(candidates.size());

  Logger().info("extract_photo_params: found {} candidate assets (only_missing={})",
                result.candidate_count, only_missing);

  if (candidates.empty()) {
    report_extract_progress(progress_callback, "completed", 0, 0, 100.0, "No candidate assets");
    return result;
  }

  ExtractProgressState progress{
      .total_candidates = result.candidate_count,
  };
  report_processing_progress(progress_callback, progress, result, true,
                             std::format("Loaded {} candidate photos", result.candidate_count));

  // 按 UID 分组批次：API 要求同一请求中所有照片属于同一用户
  std::unordered_map<std::string, std::vector<PreparedPhotoExtractEntry>> active_batches;
  active_batches.reserve(std::min<std::size_t>(candidates.size(), 256));

  for (const auto& candidate : candidates) {
    progress.scanned_count++;

    auto prepared_result = prepare_photo_extract_entry(candidate);
    if (!prepared_result) {
      mark_candidate_skipped(result, progress, candidate.id, prepared_result.error());
      report_processing_progress(progress_callback, progress, result);
      continue;
    }

    auto prepared_entry = std::move(prepared_result.value());
    auto& batch = active_batches[prepared_entry.uid];
    batch.push_back(std::move(prepared_entry));

    report_processing_progress(progress_callback, progress, result);

    if (batch.size() >= kExtractBatchSize) {
      flush_extract_batch(app_state, batch, result, progress, progress_callback);
      batch.clear();
    }
  }

  // 扫描结束后冲刷各 UID 剩余的不足一批的条目
  for (auto& [uid, batch] : active_batches) {
    (void)uid;
    flush_extract_batch(app_state, batch, result, progress, progress_callback);
  }

  Logger().info(
      "extract_photo_params: completed. candidates={}, processed={}, saved={}, skipped={}, "
      "failed={}, clothes_rows={}",
      result.candidate_count, result.processed_count, result.saved_count, result.skipped_count,
      result.failed_count, result.clothes_rows_written);

  report_extract_progress(progress_callback, "completed", result.processed_count,
                          result.candidate_count, 100.0,
                          std::format("Done: saved={}, skipped={}, failed={}", result.saved_count,
                                      result.skipped_count, result.failed_count));

  return result;
}

}  // namespace Plugins::InfinityNikki
