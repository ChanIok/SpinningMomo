#include "vendor/std.hpp"

#include "vendor/doctest.hpp"
#include "vendor/wil.hpp"
#include "vendor/windows/winioctl.hpp"

#include "utils/media/hybrid_mp4.hpp"

namespace {
using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;

auto put32(Bytes& out, std::uint32_t n) -> void {
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(std::uint8_t(n >> shift));
}
auto put64(Bytes& out, std::uint64_t n) -> void {
  put32(out, std::uint32_t(n >> 32));
  put32(out, std::uint32_t(n));
}
auto get32(View in, std::size_t at) -> std::uint32_t {
  return (std::uint32_t(in[at]) << 24) | (std::uint32_t(in[at + 1]) << 16) |
         (std::uint32_t(in[at + 2]) << 8) | in[at + 3];
}
auto get64(View in, std::size_t at) -> std::uint64_t {
  return (std::uint64_t(get32(in, at)) << 32) | get32(in, at + 4);
}
auto append(Bytes& out, View in) -> void { out.insert(out.end(), in.begin(), in.end()); }
auto box(std::string_view type, View body) -> Bytes {
  Bytes out;
  put32(out, std::uint32_t(body.size() + 8));
  out.insert(out.end(), type.begin(), type.end());
  append(out, body);
  return out;
}
auto child(View parent, std::string_view type) -> View {
  for (std::size_t at = 0; at + 8 <= parent.size();) {
    const bool wide = get32(parent, at) == 1;
    const auto size = wide ? get64(parent, at + 8) : get32(parent, at);
    if (!size || size > parent.size() - at) break;
    if (std::equal(type.begin(), type.end(), parent.begin() + at + 4))
      return parent.subspan(at + (wide ? 16 : 8), std::size_t(size) - (wide ? 16 : 8));
    at += std::size_t(size);
  }
  return {};
}
auto stbl(View file) -> View {
  return child(child(child(child(child(file, "moov"), "trak"), "mdia"), "minf"), "stbl");
}
auto make_init(std::uint32_t media_scale = 1000, std::uint32_t track_id = 1, View edits = {})
    -> Bytes {
  Bytes mvhd(24, 0), tkhd(28, 0), mdhd(24, 0);
  mvhd[14] = 3;
  mvhd[15] = 232;  // 1000 ticks/s
  for (int i = 0; i < 4; ++i) {
    tkhd[12 + i] = std::uint8_t(track_id >> (24 - 8 * i));
    mdhd[12 + i] = std::uint8_t(media_scale >> (24 - 8 * i));
  }
  // 解码配置与 HDR 描述作为不透明 sample entry 原样保留，不在收尾时重新解释。
  Bytes stsd(4, 0);
  put32(stsd, 1);
  const Bytes configuration{1, 2, 3, 4, 5, 6, 7, 8};
  append(stsd, box("hvc1", box("mdcv", configuration)));
  auto tables = box("stsd", stsd);
  append(tables, box("stsz", Bytes(12, 0)));
  Bytes mdia = box("mdhd", mdhd);
  append(mdia, box("minf", box("stbl", tables)));
  Bytes trak = box("tkhd", tkhd);
  append(trak, box("mdia", mdia));
  if (!edits.empty()) append(trak, box("edts", box("elst", edits)));
  Bytes trex(4, 0);
  for (auto value : {track_id, 1u, 100u, 4u, 0u}) put32(trex, value);
  Bytes moov = box("mvhd", mvhd);
  append(moov, box("trak", trak));
  append(moov, box("mvex", box("trex", trex)));
  return box("moov", moov);
}
auto make_fragment(std::uint64_t dts, std::int32_t first_cts = -50, std::int32_t second_cts = 0)
    -> Bytes {
  const auto make_moof = [&](std::uint32_t data_offset) {
    Bytes tfhd;
    put32(tfhd, 0x020000);
    put32(tfhd, 1);
    Bytes tfdt{1, 0, 0, 0};
    put64(tfdt, dts);
    Bytes trun;
    put32(trun, 0x01000f01);
    put32(trun, 2);
    put32(trun, data_offset);
    for (int i = 0; i < 2; ++i) {
      put32(trun, 100);
      put32(trun, 4);
      put32(trun, i ? 0x10000 : 0);
      put32(trun, std::uint32_t(i ? second_cts : first_cts));
    }
    Bytes traf = box("tfhd", tfhd);
    append(traf, box("tfdt", tfdt));
    append(traf, box("trun", trun));
    return box("moof", box("traf", traf));
  };
  auto moof = make_moof(0);
  moof = make_moof(std::uint32_t(moof.size() + 8));
  const Bytes samples{0xde, 0xad, 0xbe, 0xef, 0x11, 0x22, 0x33, 0x44};
  append(moof, box("mdat", samples));
  return moof;
}
auto fixture() -> Bytes {
  const Bytes brand{'i', 's', 'o', 'm', 0, 0, 0, 0};
  auto bytes = box("ftyp", brand);
  append(bytes, make_init());
  append(bytes, make_fragment(500));
  append(bytes, make_fragment(900, 50));
  return bytes;
}
auto save(const std::filesystem::path& path, View bytes) -> void {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  REQUIRE(stream.good());
}
auto load(const std::filesystem::path& path) -> Bytes {
  std::ifstream stream(path, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}
auto count_samples(View file) -> std::uint64_t {
  auto movie = child(file, "moov");
  std::uint64_t count = 0;
  while (true) {
    auto track = child(movie, "trak");
    if (track.empty()) break;
    const auto tables = child(child(child(track, "mdia"), "minf"), "stbl");
    const auto sizes = child(tables, "stsz");
    REQUIRE(sizes.size() >= 12);
    count += get32(sizes, 8);
    movie = movie.subspan(std::size_t(track.data() - movie.data()) + track.size());
  }
  return count;
}

// 从最终文件的 stts/ctts/elst 还原 movie 时间，而不只核对各表的独立数值。
struct Timeline {
  std::vector<std::int64_t> pts;
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
};
auto read_timeline(View movie, View track) -> Timeline {
  const auto mvhd = child(movie, "mvhd");
  const auto mdia = child(track, "mdia");
  const auto mdhd = child(mdia, "mdhd");
  const auto movie_scale = get32(mvhd, mvhd[0] ? 20 : 12);
  const auto media_scale = get32(mdhd, mdhd[0] ? 20 : 12);
  const auto tables = child(child(mdia, "minf"), "stbl");
  const auto stts = child(tables, "stts");
  const auto ctts = child(tables, "ctts");
  std::vector<std::uint32_t> durations;
  for (std::uint32_t i = 0; i < get32(stts, 4); ++i)
    durations.insert(durations.end(), get32(stts, 8 + i * 8), get32(stts, 12 + i * 8));
  std::vector<std::int64_t> offsets;
  if (!ctts.empty()) {
    for (std::uint32_t i = 0; i < get32(ctts, 4); ++i) {
      const auto raw = get32(ctts, 12 + i * 8);
      const auto offset =
          ctts[0] ? std::int64_t(std::bit_cast<std::int32_t>(raw)) : std::int64_t(raw);
      offsets.insert(offsets.end(), get32(ctts, 8 + i * 8), offset);
    }
  } else {
    offsets.resize(durations.size(), 0);
  }
  REQUIRE(offsets.size() == durations.size());
  const auto edits = child(child(track, "edts"), "elst");
  REQUIRE(edits.size() >= 28);
  REQUIRE(edits[0] == 1);
  Timeline result;
  std::int64_t media_time = 0;
  for (std::uint32_t i = 0; i < get32(edits, 4); ++i) {
    const auto duration = get64(edits, 8 + i * 20);
    const auto start = std::bit_cast<std::int64_t>(get64(edits, 16 + i * 20));
    if (start == -1) {
      result.begin += duration;
    } else {
      media_time = start;
      result.end = result.begin + duration;
    }
  }
  std::int64_t dts = 0;
  for (std::size_t i = 0; i < durations.size(); ++i) {
    result.pts.push_back(std::int64_t(result.begin) +
                         (dts + offsets[i] - media_time) * movie_scale / media_scale);
    dts += durations[i];
  }
  return result;
}

auto in_temp(const std::function<void(const std::filesystem::path&)>& action) -> void {
  static std::atomic_uint counter{0};
  const auto directory = std::filesystem::temp_directory_path() /
                         std::format("momo-hybrid-test-{}-{}-{}", GetCurrentProcessId(),
                                     GetTickCount64(), counter.fetch_add(1));
  REQUIRE(std::filesystem::create_directory(directory));
  auto cleanup = wil::scope_exit([directory] {
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
  });
  action(directory / L"sample.mp4");
}
}  // namespace

TEST_CASE("Hybrid MP4 keeps media offsets, presentation times, timing gaps and decoder metadata") {
  in_temp([](const auto& path) {
    const auto original = fixture();
    const auto original_stsd = child(stbl(original), "stsd");
    save(path, original);
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    CHECK(count_samples(load(path)) == 4);
    const auto output = load(path);
    const auto tables = stbl(output);
    REQUIRE_FALSE(tables.empty());
    CHECK(std::ranges::equal(child(tables, "stsd"), original_stsd));
    const auto stts = child(tables, "stts");
    REQUIRE(stts.size() == 32);
    CHECK(get32(stts, 4) == 3);
    CHECK(get32(stts, 20) == 300);  // 下一帧时刻不前移，前一视频样本延长至 300ms。
    const auto ctts = child(tables, "ctts");
    REQUIRE(ctts.size() >= 16);
    CHECK(ctts[0] == 0);
    CHECK(get32(ctts, 12) == 0);
    const auto movie = child(output, "moov");
    const auto timeline = read_timeline(movie, child(movie, "trak"));
    const std::vector<std::int64_t> expected{450, 600, 950, 1000};
    CHECK(timeline.pts == expected);
    CHECK(timeline.begin == 450);
    CHECK(timeline.end == 1100);
    const auto offsets = child(tables, "co64");
    REQUIRE(offsets.size() == 40);
    for (std::size_t i = 0; i < 4; ++i) {
      const auto offset = get64(offsets, 8 + i * 8);
      REQUIRE(offset + 4 <= original.size());
      CHECK(std::equal(original.begin() + offset, original.begin() + offset + 4,
                       output.begin() + offset));
    }
    CHECK(std::equal(original.begin() + 32, original.end(), output.begin() + 32));
    auto second = utils::media::hybrid_mp4::finalize(path);
    REQUIRE(second.has_value());
    CHECK(load(path) == output);
  });
}

TEST_CASE("Hybrid MP4 finalizes the same mp4 without creating companion files") {
  in_temp([](const auto& path) {
    save(path, fixture());
    REQUIRE(utils::media::hybrid_mp4::finalize(path).has_value());
    CHECK(path.extension() == L".mp4");
    CHECK(std::filesystem::exists(path));
    CHECK(std::distance(std::filesystem::directory_iterator(path.parent_path()),
                        std::filesystem::directory_iterator{}) == 1);
  });
}

TEST_CASE("Hybrid MP4 excludes an incomplete final fragment") {
  in_temp([](const auto& path) {
    auto bytes = fixture();
    auto tail = make_fragment(1500);
    tail.resize(tail.size() - 3);
    append(bytes, tail);
    save(path, bytes);
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    CHECK(count_samples(load(path)) == 4);
  });
}

TEST_CASE("Hybrid MP4 resolves two tracks, default properties and implicit run offsets") {
  in_temp([](const auto& path) {
    const auto video_init = make_init();
    const auto audio_init = make_init(48000, 2);
    const auto video_movie = child(video_init, "moov");
    const auto audio_movie = child(audio_init, "moov");
    Bytes movie = box("mvhd", child(video_movie, "mvhd"));
    append(movie, box("trak", child(video_movie, "trak")));
    append(movie, box("trak", child(audio_movie, "trak")));
    auto defaults = box("trex", child(child(video_movie, "mvex"), "trex"));
    Bytes audio_defaults(4, 0);
    for (auto value : {2u, 1u, 1024u, 2u, 0u}) put32(audio_defaults, value);
    append(defaults, box("trex", audio_defaults));
    append(movie, box("mvex", defaults));
    const Bytes brand{'i', 's', 'o', 'm', 0, 0, 0, 0};
    auto bytes = box("ftyp", brand);
    append(bytes, box("moov", movie));
    const auto make_moof = [](std::uint32_t data_offset) {
      Bytes tfhd;
      for (auto value : {0x020038u, 1u, 100u, 4u, 0x10000u}) put32(tfhd, value);
      auto video = box("tfhd", tfhd);
      Bytes tfdt{1, 0, 0, 0};
      put64(tfdt, 500);
      append(video, box("tfdt", tfdt));
      Bytes first_run;
      for (auto value : {5u, 1u, data_offset, 0u}) put32(first_run, value);
      append(video, box("trun", first_run));
      Bytes next_run;
      put32(next_run, 0);
      put32(next_run, 1);
      append(video, box("trun", next_run));
      Bytes audio_header;
      put32(audio_header, 0);  // 后一 traf 继承前一 traf 的数据末端作为 base。
      put32(audio_header, 2);
      auto audio = box("tfhd", audio_header);
      Bytes audio_run;
      put32(audio_run, 0);
      put32(audio_run, 2);
      append(audio, box("trun", audio_run));
      auto payload = box("traf", video);
      append(payload, box("traf", audio));
      return box("moof", payload);
    };
    auto fragment = make_moof(0);
    fragment = make_moof(std::uint32_t(fragment.size() + 8));
    const auto first_position = bytes.size() + fragment.size() + 8;
    append(bytes, fragment);
    append(bytes, box("mdat", Bytes(12, 0x42)));
    save(path, bytes);
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    CHECK(count_samples(load(path)) == 4);
    const auto output = load(path);
    const auto rebuilt_movie = child(output, "moov");
    const auto video = child(rebuilt_movie, "trak");
    const auto next_track_at = std::size_t(video.data() - rebuilt_movie.data()) + video.size();
    const auto audio = child(rebuilt_movie.subspan(next_track_at), "trak");
    REQUIRE_FALSE(audio.empty());
    const auto video_tables = child(child(child(video, "mdia"), "minf"), "stbl");
    const auto audio_tables = child(child(child(audio, "mdia"), "minf"), "stbl");
    CHECK(get64(child(video_tables, "co64"), 8) == first_position);
    CHECK(get64(child(video_tables, "co64"), 16) == first_position + 4);
    CHECK(get64(child(audio_tables, "co64"), 8) == first_position + 8);
    CHECK(get64(child(audio_tables, "co64"), 16) == first_position + 10);
    CHECK(get32(child(video_tables, "stts"), 12) == 100);
    CHECK(get32(child(audio_tables, "stts"), 12) == 1024);
    CHECK(get32(child(video_tables, "stss"), 4) == 1);
    CHECK(get32(child(video_tables, "stss"), 8) == 1);
    CHECK(get32(child(rebuilt_movie, "mvhd"), 12) == 48000);
    CHECK(get64(child(child(video, "edts"), "elst"), 8) == 24000);
    CHECK(get64(child(child(audio, "edts"), "elst"), 8) == 2048);
    const std::vector<std::int64_t> video_pts{24000, 28800};
    const std::vector<std::int64_t> audio_pts{0, 1024};
    CHECK(read_timeline(rebuilt_movie, video).pts == video_pts);
    CHECK(read_timeline(rebuilt_movie, audio).pts == audio_pts);
  });
}

TEST_CASE("Hybrid MP4 preserves a fractional-millisecond start using the movie timescale") {
  in_temp([](const auto& path) {
    const Bytes brand{'i', 's', 'o', 'm', 0, 0, 0, 0};
    auto bytes = box("ftyp", brand);
    append(bytes, make_init(30000));
    append(bytes, make_fragment(1001, 0));
    save(path, bytes);
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    const auto output = load(path);
    const auto movie = child(output, "moov");
    CHECK(get32(child(movie, "mvhd"), 12) == 30000);
    const auto edits = child(child(child(movie, "trak"), "edts"), "elst");
    REQUIRE(edits.size() == 48);
    CHECK(get64(edits, 8) == 1001);
  });
}

TEST_CASE("Hybrid MP4 preserves the presentation delay introduced by frame reordering") {
  in_temp([](const auto& path) {
    const Bytes brand{'i', 's', 'o', 'm', 0, 0, 0, 0};
    auto bytes = box("ftyp", brand);
    append(bytes, make_init());
    append(bytes, make_fragment(0, 50));
    save(path, bytes);
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    const auto output = load(path);
    const auto edits = child(child(child(child(output, "moov"), "trak"), "edts"), "elst");
    REQUIRE(edits.size() == 48);
    CHECK(get64(edits, 8) == 50);   // 50ms 显式空段，首个 PTS 不归零。
    CHECK(get64(edits, 36) == 50);  // 同量 media_time 抵消空段，不平移 DTS。
    CHECK(child(stbl(output), "ctts")[0] == 0);
    const auto movie = child(output, "moov");
    const auto timeline = read_timeline(movie, child(movie, "trak"));
    const std::vector<std::int64_t> expected{50, 100};
    CHECK(timeline.pts == expected);
    CHECK(timeline.begin == 50);
    CHECK(timeline.end == 200);
  });
}

TEST_CASE("Hybrid MP4 keeps signed CTS when later samples precede their decode time") {
  in_temp([](const auto& path) {
    auto bytes = box("ftyp", Bytes{'i', 's', 'o', 'm', 0, 0, 0, 0});
    append(bytes, make_init());
    append(bytes, make_fragment(500, 0, -50));
    save(path, bytes);
    REQUIRE(utils::media::hybrid_mp4::finalize(path).has_value());
    const auto output = load(path);
    const auto ctts = child(stbl(output), "ctts");
    REQUIRE(ctts.size() == 24);
    CHECK(ctts[0] == 1);
    CHECK(std::bit_cast<std::int32_t>(get32(ctts, 20)) == -50);
    const auto movie = child(output, "moov");
    const std::vector<std::int64_t> expected{500, 550};
    CHECK(read_timeline(movie, child(movie, "trak")).pts == expected);
  });
}

TEST_CASE("Hybrid MP4 preserves an existing edit offset with an unknown final duration") {
  in_temp([](const auto& path) {
    Bytes edits{1, 0, 0, 0};
    put32(edits, 2);
    put64(edits, 20);
    put64(edits, UINT64_MAX);
    put32(edits, 0x10000);
    put64(edits, 0);
    put64(edits, 450);
    put32(edits, 0x10000);
    auto bytes = box("ftyp", Bytes{'i', 's', 'o', 'm', 0, 0, 0, 0});
    append(bytes, make_init(1000, 1, edits));
    append(bytes, make_fragment(500));
    append(bytes, make_fragment(900, 50));
    save(path, bytes);
    REQUIRE(utils::media::hybrid_mp4::finalize(path).has_value());
    const auto output = load(path);
    const auto movie = child(output, "moov");
    const auto timeline = read_timeline(movie, child(movie, "trak"));
    const std::vector<std::int64_t> expected{20, 170, 520, 570};
    CHECK(timeline.pts == expected);
    CHECK(timeline.begin == 20);
    CHECK(timeline.end == 670);
  });
}

TEST_CASE("Hybrid MP4 preserves both boundaries of a finite edit after rescaling") {
  in_temp([](const auto& path) {
    // movie 为 1000Hz，media 为 30000Hz；空段 1ms，有效媒体只播放 3ms。
    Bytes edits{1, 0, 0, 0};
    put32(edits, 2);
    put64(edits, 1);
    put64(edits, UINT64_MAX);
    put32(edits, 0x10000);
    put64(edits, 3);
    put64(edits, 1101);
    put32(edits, 0x10000);
    auto bytes = box("ftyp", Bytes{'i', 's', 'o', 'm', 0, 0, 0, 0});
    append(bytes, make_init(30000, 1, edits));
    append(bytes, make_fragment(1001, 0));
    save(path, bytes);
    REQUIRE(utils::media::hybrid_mp4::finalize(path).has_value());
    const auto output = load(path);
    const auto movie = child(output, "moov");
    const auto timeline = read_timeline(movie, child(movie, "trak"));
    const std::vector<std::int64_t> expected{-70, 30};
    CHECK(timeline.pts == expected);
    CHECK(timeline.begin == 30);
    CHECK(timeline.end == 120);
    CHECK(get32(child(movie, "mvhd"), 16) == 120);
    CHECK(count_samples(output) == 2);  // edit 裁剪展示区间，不删除解码样本。
  });
}

TEST_CASE("Hybrid MP4 leaves the original fragments intact when an edit excludes all media") {
  in_temp([](const auto& path) {
    Bytes edits{1, 0, 0, 0};
    put32(edits, 1);
    put64(edits, 10);
    put64(edits, 10000);
    put32(edits, 0x10000);
    auto bytes = box("ftyp", Bytes{'i', 's', 'o', 'm', 0, 0, 0, 0});
    append(bytes, make_init(1000, 1, edits));
    append(bytes, make_fragment(500, 0));
    save(path, bytes);
    CHECK_FALSE(utils::media::hybrid_mp4::finalize(path).has_value());
    CHECK(load(path) == bytes);
  });
}

TEST_CASE("Hybrid MP4 recovers a prefix when the last mdat uses an open-ended size") {
  in_temp([](const auto& path) {
    auto bytes = fixture();
    auto tail = make_fragment(1500);
    const auto mdat_at = tail.size() - 16;
    std::fill(tail.begin() + mdat_at, tail.begin() + mdat_at + 4, 0);
    tail.resize(tail.size() - 3);
    append(bytes, tail);
    save(path, bytes);
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    CHECK(count_samples(load(path)) == 4);
  });
}

TEST_CASE("Hybrid MP4 upgrades duration headers without losing long decode timelines") {
  in_temp([](const auto& path) {
    const Bytes brand{'i', 's', 'o', 'm', 0, 0, 0, 0};
    auto bytes = box("ftyp", brand);
    append(bytes, make_init());
    for (std::uint64_t time : {0ULL, 3'000'000'000ULL, 6'000'000'000ULL})
      append(bytes, make_fragment(time, 0));
    save(path, bytes);
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    const auto output = load(path);
    const auto movie = child(output, "moov");
    const auto mvhd = child(movie, "mvhd");
    REQUIRE(mvhd.size() >= 32);
    CHECK(mvhd[0] == 1);
    CHECK(get64(mvhd, 24) == 6'000'000'200ULL);
  });
}

TEST_CASE("Hybrid MP4 rejects malformed metadata without changing the recording") {
  in_temp([](const auto& path) {
    auto bytes = fixture();
    bytes[16] = 0xff;  // 初始化 moov 声明越界，不存在可恢复的初始化信息
    save(path, bytes);
    CHECK_FALSE(utils::media::hybrid_mp4::finalize(path).has_value());
    CHECK(load(path) == bytes);
  });
}

TEST_CASE("Hybrid MP4 does not rewrite a recording while a writer still owns it") {
  in_temp([](const auto& path) {
    const auto original = fixture();
    save(path, original);
    wil::unique_hfile writer(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    REQUIRE(writer);
    CHECK_FALSE(utils::media::hybrid_mp4::finalize(path).has_value());
    writer.reset();
    CHECK(load(path) == original);
  });
}

TEST_CASE("Hybrid MP4 does not discard samples already indexed by the initialization moov") {
  in_temp([](const auto& path) {
    auto bytes = fixture();
    const auto initial_sizes = child(stbl(bytes), "stsz");
    REQUIRE(initial_sizes.size() == 12);
    bytes[std::size_t(initial_sizes.data() - bytes.data()) + 11] = 1;
    save(path, bytes);
    CHECK_FALSE(utils::media::hybrid_mp4::finalize(path).has_value());
    CHECK(load(path) == bytes);
  });
}

TEST_CASE("Hybrid MP4 uses 64-bit media sizes and sample positions beyond 4 GiB") {
  in_temp([](const auto& path) {
    wil::unique_hfile file(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    REQUIRE(file);
    DWORD ignored = 0;
    if (!DeviceIoControl(file.get(), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &ignored, nullptr)) {
      WARN_MESSAGE(false, "Sparse files unavailable; >4 GiB fixture was not exercised");
      return;
    }
    const Bytes brand{'i', 's', 'o', 'm', 0, 0, 0, 0};
    auto prefix = box("ftyp", brand);
    append(prefix, make_init());
    constexpr std::uint64_t gap = (std::uint64_t(1) << 32) + 16;
    Bytes free;
    put32(free, 1);
    append(free, Bytes{'f', 'r', 'e', 'e'});
    put64(free, gap);
    REQUIRE(WriteFile(file.get(), prefix.data(), DWORD(prefix.size()), &ignored, nullptr));
    REQUIRE(WriteFile(file.get(), free.data(), DWORD(free.size()), &ignored, nullptr));
    LARGE_INTEGER position{};
    position.QuadPart = LONGLONG(prefix.size() + gap);
    REQUIRE(SetFilePointerEx(file.get(), position, nullptr, FILE_BEGIN));
    const auto tail = make_fragment(0, 0);
    REQUIRE(WriteFile(file.get(), tail.data(), DWORD(tail.size()), &ignored, nullptr));
    file.reset();
    auto result = utils::media::hybrid_mp4::finalize(path);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    std::ifstream stream(path, std::ios::binary);
    stream.seekg(16);
    std::array<std::uint8_t, 16> header{};
    stream.read(reinterpret_cast<char*>(header.data()), header.size());
    CHECK(get32(header, 0) == 1);
    CHECK(get64(header, 8) > UINT32_MAX);
    // 只读取追加的小索引，不把超过 4GiB 的稀疏媒体区载入内存。
    stream.seekg(static_cast<std::streamoff>(position.QuadPart) +
                 static_cast<std::streamoff>(tail.size()));
    std::array<std::uint8_t, 8> moov_header{};
    stream.read(reinterpret_cast<char*>(moov_header.data()), moov_header.size());
    REQUIRE(stream.good());
    const auto moov_size = get32(moov_header, 0);
    REQUIRE(moov_size >= 8);
    REQUIRE(moov_size < 1024 * 1024);
    Bytes metadata(moov_size);
    std::copy(moov_header.begin(), moov_header.end(), metadata.begin());
    stream.read(reinterpret_cast<char*>(metadata.data() + 8), moov_size - 8);
    REQUIRE(stream.good());
    CHECK(count_samples(metadata) == 2);
    CHECK(get64(child(stbl(metadata), "co64"), 8) > UINT32_MAX);
  });
}
