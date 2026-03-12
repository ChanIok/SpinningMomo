module;

export module Extensions.InfinityNikki.PhotoExtract.Scan;

import std;

export namespace Extensions::InfinityNikki::PhotoExtract::Scan {

struct CandidateAssetRow {
  std::int64_t id;
  std::string path;
};

struct PreparedPhotoExtractEntry {
  std::int64_t asset_id;
  std::string uid;
  std::string params_base64;
};

auto prepare_photo_extract_entry(const CandidateAssetRow& candidate)
    -> std::expected<PreparedPhotoExtractEntry, std::string>;

}  // namespace Extensions::InfinityNikki::PhotoExtract::Scan
