module;

#include <asio.hpp>

export module Extensions.InfinityNikki.PhotoExtract.Infra;

import std;
import Core.State;
import Extensions.InfinityNikki.PhotoExtract.Scan;
import Extensions.InfinityNikki.Types;

export namespace Extensions::InfinityNikki::PhotoExtract::Infra {

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

struct ParsedPhotoParamsBatchItem {
  std::int64_t asset_id;
  ParsedPhotoParamsRecord record;
};

auto load_candidate_assets(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::vector<Scan::CandidateAssetRow>, std::string>;

auto extract_batch_photo_params(Core::State::AppState& app_state,
                                const std::vector<Scan::PreparedPhotoExtractEntry>& entries)
    -> asio::awaitable<
        std::expected<std::vector<std::optional<ParsedPhotoParamsRecord>>, std::string>>;

auto upsert_photo_params_batch(Core::State::AppState& app_state, const std::string& uid,
                               const std::vector<ParsedPhotoParamsBatchItem>& items)
    -> std::expected<std::int32_t, std::string>;

}  // namespace Extensions::InfinityNikki::PhotoExtract::Infra
