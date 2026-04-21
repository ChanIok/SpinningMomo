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
  std::optional<std::int64_t> time_hour;
  std::optional<std::int64_t> time_min;
  std::optional<double> camera_focal_length;
  std::optional<double> rotation;
  std::optional<double> aperture_value;
  std::optional<std::int64_t> filter_id;
  std::optional<double> filter_strength;
  std::optional<double> vignette_intensity;
  std::optional<std::int64_t> light_id;
  std::optional<double> light_strength;
  std::optional<bool> vertical;
  std::optional<double> bloom_intensity;
  std::optional<double> bloom_threshold;
  std::optional<double> brightness;
  std::optional<double> exposure;
  std::optional<double> contrast;
  std::optional<double> saturation;
  std::optional<double> vibrance;
  std::optional<double> highlights;
  std::optional<double> shadow;
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

struct ExtractBatchPhotoParamsRecord {
  std::optional<ParsedPhotoParamsRecord> record;
  std::optional<std::string> error_message;
};

auto load_candidate_assets(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::vector<Scan::CandidateAssetRow>, std::string>;

auto load_candidate_assets_by_ids(Core::State::AppState& app_state,
                                  const std::vector<std::int64_t>& candidate_asset_ids)
    -> std::expected<std::vector<Scan::CandidateAssetRow>, std::string>;

auto extract_batch_photo_params(Core::State::AppState& app_state,
                                const std::vector<Scan::PreparedPhotoExtractEntry>& entries)
    -> asio::awaitable<std::expected<std::vector<ExtractBatchPhotoParamsRecord>, std::string>>;

auto upsert_photo_params_batch(Core::State::AppState& app_state, const std::string& uid,
                               const std::vector<ParsedPhotoParamsBatchItem>& items)
    -> std::expected<std::int32_t, std::string>;

}  // namespace Extensions::InfinityNikki::PhotoExtract::Infra
