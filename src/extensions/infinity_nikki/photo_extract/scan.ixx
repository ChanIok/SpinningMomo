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
  std::string md5;
  std::string encoded;
};

auto prepare_photo_extract_entry(const CandidateAssetRow& candidate,
                                 const std::optional<std::string>& uid_override = std::nullopt)
    -> std::expected<PreparedPhotoExtractEntry, std::string>;

}  // namespace Extensions::InfinityNikki::PhotoExtract::Scan
