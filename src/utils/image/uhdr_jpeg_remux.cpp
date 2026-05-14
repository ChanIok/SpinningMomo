module Utils.Image.UhdrJpegRemux;

import std;

namespace Utils::Image {

namespace {

constexpr std::uint8_t kJpegStart = 0xFF;

[[nodiscard]] auto iter_pre_sos_raw_segments(std::span<const std::uint8_t> data)
    -> std::expected<std::pair<std::size_t, std::vector<std::vector<std::uint8_t>>>, std::string> {
  if (data.size() < 4 || data[0] != kJpegStart || data[1] != 0xD8) {
    return std::unexpected(std::string{"invalid JPEG SOI"});
  }
  std::size_t i = 2;
  std::vector<std::vector<std::uint8_t>> segs;
  std::optional<std::size_t> sos_start;
  while (i + 1 < data.size()) {
    if (data[i] != kJpegStart) {
      ++i;
      continue;
    }
    const std::size_t start = i;
    while (i < data.size() && data[i] == kJpegStart) {
      ++i;
    }
    if (i >= data.size()) {
      break;
    }
    const std::uint8_t marker = data[i];
    ++i;
    if (marker == 0xD8 || marker == 0xD9) {
      continue;
    }
    if (marker == 0xDA) {
      sos_start = start;
      break;
    }
    if (i + 2 > data.size()) {
      return std::unexpected(std::string{"JPEG truncated before SOS"});
    }
    const std::size_t seg_len =
        (static_cast<std::size_t>(data[i]) << 8) | static_cast<std::size_t>(data[i + 1]);
    if (seg_len < 2 || i + seg_len > data.size()) {
      return std::unexpected(std::string{"invalid JPEG segment length"});
    }
    const std::size_t total = 2 + seg_len;
    segs.emplace_back(data.begin() + static_cast<std::ptrdiff_t>(start),
                      data.begin() + static_cast<std::ptrdiff_t>(start + total));
    i += seg_len;
  }
  if (!sos_start.has_value()) {
    return std::unexpected(std::string{"SOS not found"});
  }
  return std::pair{*sos_start, std::move(segs)};
}

[[nodiscard]] auto is_app_segment(std::span<const std::uint8_t> seg) -> bool {
  if (seg.size() < 4 || seg[0] != kJpegStart) {
    return false;
  }
  const std::uint8_t m = seg[1];
  return m >= 0xE0 && m <= 0xEF;
}

[[nodiscard]] auto classify_app_payload(std::span<const std::uint8_t> pl) -> std::string_view {
  if (pl.size() >= 5 && pl[0] == 'J' && pl[1] == 'F' && pl[2] == 'I' && pl[3] == 'F' &&
      pl[4] == 0) {
    return "jfif";
  }
  if (pl.size() >= 22 && std::memcmp(pl.data(), "http://ns.adobe.com/xap/", 22) == 0) {
    return "xmp";
  }
  if (pl.size() >= 6 && pl[0] == 'E' && pl[1] == 'x' && pl[2] == 'i' && pl[3] == 'f' &&
      pl[4] == 0 && pl[5] == 0) {
    return "exif";
  }
  if (pl.size() >= 12 && std::memcmp(pl.data(), "ICC_PROFILE\x00", 12) == 0) {
    return "icc";
  }
  if (pl.size() >= 4 && std::memcmp(pl.data(), "MPF\x00", 4) == 0) {
    return "mpf";
  }
  constexpr std::string_view kIso = "urn:iso:std:iso:ts:21496";
  const std::size_t scan = std::min(pl.size(), std::size_t{120});
  for (std::size_t j = 0; j + kIso.size() <= scan; ++j) {
    if (std::memcmp(pl.data() + j, kIso.data(), kIso.size()) == 0) {
      return "iso21496";
    }
  }
  return "other";
}

// Ultra HDR 仅需前两枚 SOI 定界；找到第二枚即停，避免扫完整张超大 JPEG。
[[nodiscard]] auto find_first_two_soi(std::span<const std::uint8_t> data)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> out;
  out.reserve(2);
  std::size_t k = 0;
  while (k + 1 < data.size() && out.size() < 2) {
    if (data[k] == kJpegStart && data[k + 1] == 0xD8) {
      out.push_back(k);
      k += 2;
    } else {
      ++k;
    }
  }
  return out;
}

[[nodiscard]] auto patch_mpf_mandatory_fields(const std::vector<std::uint8_t>& mpf_segment,
                                              std::uint32_t primary_image_size,
                                              std::uint32_t secondary_image_offset)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  if (mpf_segment.size() < 4 + 54 + 16 + 8 || mpf_segment[0] != kJpegStart ||
      mpf_segment[1] != 0xE2) {
    return std::unexpected(std::string{"MPF segment invalid"});
  }
  const auto* payload = mpf_segment.data() + 4;
  if (std::memcmp(payload, "MPF\x00", 4) != 0) {
    return std::unexpected(std::string{"MPF signature missing"});
  }
  constexpr std::size_t kMpEntries = 54;
  std::vector<std::uint8_t> b = mpf_segment;
  const std::size_t pay_pri = kMpEntries + 4;
  const std::size_t off_pri = 4 + pay_pri;
  b[off_pri + 0] = static_cast<std::uint8_t>((primary_image_size >> 24) & 0xFF);
  b[off_pri + 1] = static_cast<std::uint8_t>((primary_image_size >> 16) & 0xFF);
  b[off_pri + 2] = static_cast<std::uint8_t>((primary_image_size >> 8) & 0xFF);
  b[off_pri + 3] = static_cast<std::uint8_t>(primary_image_size & 0xFF);
  const std::size_t pay_sec_off = kMpEntries + 16 + 8;
  const std::size_t off_sec = 4 + pay_sec_off;
  b[off_sec + 0] = static_cast<std::uint8_t>((secondary_image_offset >> 24) & 0xFF);
  b[off_sec + 1] = static_cast<std::uint8_t>((secondary_image_offset >> 16) & 0xFF);
  b[off_sec + 2] = static_cast<std::uint8_t>((secondary_image_offset >> 8) & 0xFF);
  b[off_sec + 3] = static_cast<std::uint8_t>(secondary_image_offset & 0xFF);
  return b;
}

}  // namespace

auto remux_uhdr_jpeg_app_order(std::span<const std::uint8_t> jpeg)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  auto pre = iter_pre_sos_raw_segments(jpeg);
  if (!pre) {
    return std::unexpected(pre.error());
  }
  const std::size_t sos_start = pre->first;
  const auto& pre_sos = pre->second;

  std::size_t first_non_app = pre_sos.size();
  for (std::size_t idx = 0; idx < pre_sos.size(); ++idx) {
    if (!is_app_segment(std::span<const std::uint8_t>(pre_sos[idx].data(), pre_sos[idx].size()))) {
      first_non_app = idx;
      break;
    }
  }
  std::vector<std::vector<std::uint8_t>> app_segs(
      pre_sos.begin(), pre_sos.begin() + static_cast<std::ptrdiff_t>(first_non_app));
  std::vector<std::uint8_t> tail_meta;
  for (std::size_t idx = first_non_app; idx < pre_sos.size(); ++idx) {
    tail_meta.insert(tail_meta.end(), pre_sos[idx].begin(), pre_sos[idx].end());
  }

  const auto sois = find_first_two_soi(jpeg);
  if (sois.size() < 2) {
    return std::unexpected(std::string{"need two SOI markers"});
  }
  const std::size_t sec_abs = sois[1];

  std::map<std::string, std::vector<std::uint8_t>, std::less<>> slot;
  std::vector<std::vector<std::uint8_t>> others;
  std::vector<std::string> order_first;

  for (const auto& raw : app_segs) {
    if (raw.size() < 4) {
      return std::unexpected(std::string{"APP segment too short"});
    }
    const std::span<const std::uint8_t> pl(raw.data() + 4, raw.size() - 4);
    const auto c = classify_app_payload(pl);
    if (c == "other") {
      others.push_back(raw);
      order_first.push_back("other:" + std::to_string(others.size() - 1));
      continue;
    }
    if (slot.contains(std::string(c))) {
      return std::unexpected(std::string{"duplicate APP type: "} + std::string(c));
    }
    slot.insert_or_assign(std::string(c), std::vector<std::uint8_t>(raw.begin(), raw.end()));
    order_first.push_back(std::string(c));
  }
  if (!slot.contains("mpf")) {
    return std::unexpected(std::string{"MPF missing"});
  }

  static constexpr std::array<std::string_view, 6> kBase = {
      "jfif", "exif", "xmp", "icc", "iso21496", "mpf",
  };
  std::vector<std::vector<std::uint8_t>> new_parts;
  std::set<std::string, std::less<>> used;
  for (const auto& k : kBase) {
    const auto it = slot.find(std::string{k});
    if (it != slot.end()) {
      new_parts.push_back(it->second);
      used.insert(it->first);
    }
  }
  for (const auto& k : order_first) {
    if (k.starts_with("other:")) {
      const auto idx = static_cast<std::size_t>(std::stoul(k.substr(6)));
      new_parts.push_back(others.at(idx));
    } else if (!used.contains(k)) {
      new_parts.push_back(slot.at(k));
      used.insert(k);
    }
  }

  const std::size_t old_meta_begin = 2;
  const std::size_t old_meta_len = sos_start - old_meta_begin;

  std::size_t mpf_meta_off = 0;
  std::size_t cur = 0;
  bool found_mpf = false;
  for (const auto& p : new_parts) {
    if (p.size() > 8 && p[0] == kJpegStart && p[1] == 0xE2 &&
        std::memcmp(p.data() + 4, "MPF\x00", 4) == 0) {
      mpf_meta_off = cur;
      found_mpf = true;
      break;
    }
    cur += p.size();
  }
  if (!found_mpf) {
    return std::unexpected(std::string{"MPF not in new_parts"});
  }

  std::vector<std::uint8_t> new_meta;
  new_meta.reserve(sos_start + tail_meta.size() + 256);
  for (const auto& p : new_parts) {
    const bool is_mpf = p.size() > 8 && p[0] == kJpegStart && p[1] == 0xE2 &&
                        std::memcmp(p.data() + 4, "MPF\x00", 4) == 0;
    if (!is_mpf) {
      new_meta.insert(new_meta.end(), p.begin(), p.end());
      continue;
    }
    std::size_t blob_apps = 0;
    for (const auto& q : new_parts) {
      blob_apps += q.size();
    }
    const std::size_t new_meta_local_len = blob_apps + tail_meta.size();
    const std::ptrdiff_t delta_local =
        static_cast<std::ptrdiff_t>(new_meta_local_len) - static_cast<std::ptrdiff_t>(old_meta_len);
    const std::ptrdiff_t sec_signed = static_cast<std::ptrdiff_t>(sec_abs) + delta_local;
    if (sec_signed < 0) {
      return std::unexpected(std::string{"secondary SOI offset invalid after remux"});
    }
    const std::size_t sec_abs_new = static_cast<std::size_t>(sec_signed);
    const std::uint32_t primary_image_size = static_cast<std::uint32_t>(sec_abs_new);
    const std::uint32_t mm_file = static_cast<std::uint32_t>(2 + mpf_meta_off + 8);
    const std::uint32_t secondary_image_offset =
        static_cast<std::uint32_t>(sec_abs_new >= mm_file ? sec_abs_new - mm_file : 0);
    auto patched = patch_mpf_mandatory_fields(p, primary_image_size, secondary_image_offset);
    if (!patched) {
      return std::unexpected(patched.error());
    }
    new_meta.insert(new_meta.end(), patched->begin(), patched->end());
  }
  new_meta.insert(new_meta.end(), tail_meta.begin(), tail_meta.end());

  std::vector<std::uint8_t> out;
  out.reserve(jpeg.size());
  out.push_back(jpeg[0]);
  out.push_back(jpeg[1]);
  out.insert(out.end(), new_meta.begin(), new_meta.end());
  out.insert(out.end(), jpeg.begin() + static_cast<std::ptrdiff_t>(sos_start),
             jpeg.begin() + static_cast<std::ptrdiff_t>(sec_abs));
  out.insert(out.end(), jpeg.begin() + static_cast<std::ptrdiff_t>(sec_abs), jpeg.end());
  return out;
}

}  // namespace Utils::Image
