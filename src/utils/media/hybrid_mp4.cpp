#include "utils/media/hybrid_mp4.hpp"

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"

namespace utils::media::hybrid_mp4 {
namespace {

using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr auto tag(char a, char b, char c, char d) -> std::uint32_t {
  return (std::uint32_t(a) << 24) | (std::uint32_t(b) << 16) | (std::uint32_t(c) << 8) |
         std::uint32_t(d);
}
constexpr auto moov_tag = tag('m', 'o', 'o', 'v');
constexpr auto mdat_tag = tag('m', 'd', 'a', 't');

auto u32(View data, std::size_t at) -> std::uint32_t {
  return (std::uint32_t(data[at]) << 24) | (std::uint32_t(data[at + 1]) << 16) |
         (std::uint32_t(data[at + 2]) << 8) | data[at + 3];
}
auto u64(View data, std::size_t at) -> std::uint64_t {
  return (std::uint64_t(u32(data, at)) << 32) | u32(data, at + 4);
}
auto put32(Bytes& out, std::uint32_t value) -> void {
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(std::uint8_t(value >> shift));
}
auto put64(Bytes& out, std::uint64_t value) -> void {
  put32(out, std::uint32_t(value >> 32));
  put32(out, std::uint32_t(value));
}
auto set32(Bytes& out, std::size_t at, std::uint32_t value) -> void {
  for (int i = 0; i < 4; ++i) out[at + i] = std::uint8_t(value >> (24 - i * 8));
}
auto set64(Bytes& out, std::size_t at, std::uint64_t value) -> void {
  set32(out, at, std::uint32_t(value >> 32));
  set32(out, at + 4, std::uint32_t(value));
}
auto append(Bytes& out, View data) -> void { out.insert(out.end(), data.begin(), data.end()); }

// 打包生成 MP4 Box：根据载荷大小决定采用标准 8 字节头还是 64 位超长头
auto box(std::uint32_t type, View payload) -> Bytes {
  Bytes out;
  // 载荷未超 32 位上限时写入标准 8 字节头部
  if (payload.size() <= UINT32_MAX - 8) {
    put32(out, std::uint32_t(payload.size() + 8));
    put32(out, type);
  } else {
    // 超过 32 位上限时写入 64 位扩展长度头（size = 1）
    put32(out, 1);
    put32(out, type);
    put64(out, payload.size() + 16);
  }
  append(out, payload);
  return out;
}

struct Box {
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
  std::uint32_t type = 0;
  std::uint32_t header = 8;
};

// 解析单个 MP4 Box 头部：读取偏移量、四字符类型及 32/64 位长度并校验边界
auto parse_box(View data, std::size_t at) -> Result<Box> {
  // 剩余数据不足最小 8 字节头时直接报错
  if (at > data.size() || data.size() - at < 8) return std::unexpected("Truncated MP4 box");
  Box value{.offset = at, .size = u32(data, at), .type = u32(data, at + 4)};
  // 长度标记为 1 时说明后接 64 位超长尺寸
  if (value.size == 1) {
    if (data.size() - at < 16) return std::unexpected("Truncated extended MP4 box");
    value.size = u64(data, at + 8);
    value.header = 16;
  } else if (value.size == 0) {
    // 长度为 0 表示 Box 持续到当前载荷末尾
    value.size = data.size() - at;
  }
  // 校验 Box 长度不能小于头部且不能越界
  if (value.size < value.header || value.size > data.size() - at)
    return std::unexpected("Invalid MP4 box extent");
  return value;
}

// 顺序遍历并解析容器 Box 内部所有的直接子 Box
auto children(View payload) -> Result<std::vector<Box>> {
  std::vector<Box> out;
  for (std::size_t at = 0; at < payload.size();) {
    // 逐个步进解析子 Box 头部
    auto item = parse_box(payload, at);
    if (!item) return std::unexpected(item.error());
    out.push_back(*item);
    at += std::size_t(item->size);
  }
  return out;
}

auto content(View data, const Box& item) -> View {
  return data.subspan(std::size_t(item.offset) + item.header, std::size_t(item.size) - item.header);
}

// 在子节点中查找指定四字符类型的首个 Box 并返回其载荷视图
auto find(View data, std::uint32_t type) -> Result<View> {
  auto list = children(data);
  if (!list) return std::unexpected(list.error());
  for (const auto& item : *list)
    if (item.type == type) return content(data, item);
  return std::unexpected("Required MP4 box is missing");
}

struct File {
  wil::unique_hfile handle;
  std::uint64_t size = 0;
};
auto io_error(std::string_view operation) -> std::string {
  return std::format("{} failed: Win32 error {}", operation, GetLastError());
}
// 以独占写共享读模式打开录制文件，并获取当前文件尺寸
auto open_file(const std::filesystem::path& path) -> Result<File> {
  File file;
  // 不允许与尚未结束的录制 writer 同时改写。
  file.handle.reset(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file.handle) return std::unexpected(io_error("Open recording"));
  // 查询文件 64 位实际长度
  LARGE_INTEGER length{};
  if (!GetFileSizeEx(file.handle.get(), &length)) return std::unexpected(io_error("Get file size"));
  file.size = std::uint64_t(length.QuadPart);
  return file;
}

// 移动文件指针到指定的绝对偏移位置
auto seek(File& file, std::uint64_t offset) -> Result<void> {
  if (offset > INT64_MAX) return std::unexpected("MP4 file offset exceeds signed 64-bit range");
  LARGE_INTEGER position{};
  position.QuadPart = static_cast<LONGLONG>(offset);
  // 执行 Win32 文件指针跳转
  if (!SetFilePointerEx(file.handle.get(), position, nullptr, FILE_BEGIN))
    return std::unexpected(io_error("Seek recording"));
  return {};
}

// 从指定偏移位置分块读取二进制数据到内存
auto read(File& file, std::uint64_t offset, std::size_t count) -> Result<Bytes> {
  // 防止越过已知文件边界读取
  if (offset > file.size || count > file.size - offset)
    return std::unexpected("Read outside recording bounds");
  auto positioned = seek(file, offset);
  if (!positioned) return std::unexpected(positioned.error());
  Bytes bytes(count);
  // 按最大 1MB 分块循环读取，避免单次超大 I/O 失败
  for (std::size_t at = 0; at < count;) {
    DWORD received = 0;
    const auto request = static_cast<DWORD>(std::min<std::size_t>(count - at, 1024 * 1024));
    if (!ReadFile(file.handle.get(), bytes.data() + at, request, &received, nullptr) || !received)
      return std::unexpected(io_error("Read recording"));
    at += received;
  }
  return bytes;
}

// 向指定偏移处分块写入数据，并同步更新记录的文件大小
auto write(File& file, std::uint64_t offset, View bytes) -> Result<void> {
  auto positioned = seek(file, offset);
  if (!positioned) return positioned;
  // 按最大 1MB 分块循环写入磁盘
  for (std::size_t at = 0; at < bytes.size();) {
    DWORD written = 0;
    const auto count = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - at, 1024 * 1024));
    if (!WriteFile(file.handle.get(), bytes.data() + at, count, &written, nullptr) || !written)
      return std::unexpected(io_error("Write recording"));
    at += written;
  }
  // 写入超出原长度时拓展文件记录尺寸
  file.size = std::max(file.size, offset + bytes.size());
  return {};
}

// 强制将文件缓冲区刷入物理磁盘
auto flush(File& file) -> Result<void> {
  if (!FlushFileBuffers(file.handle.get())) return std::unexpected(io_error("Flush recording"));
  return {};
}

// 直接从磁盘嗅探指定偏移处的顶层 Box 头部（无需预先读入载荷）
auto top_box(File& file, std::uint64_t offset) -> Result<Box> {
  if (offset > file.size || file.size - offset < 8) return Box{.offset = offset};
  // 预读最多 16 字节检测 32 位或 64 位 Box 长度
  auto bytes = read(file, offset, std::size_t(std::min<std::uint64_t>(16, file.size - offset)));
  if (!bytes) return std::unexpected(bytes.error());
  Box item{.offset = offset, .size = u32(*bytes, 0), .type = u32(*bytes, 4)};
  if (item.size == 1) {
    // 尺寸为 1 时说明是 64 位扩展头，提取真实大尺寸
    if (bytes->size() < 16) return Box{.offset = offset};
    item.size = u64(*bytes, 8);
    item.header = 16;
  } else if (item.size == 0) {
    // 尺寸为 0 表示延伸至文件末尾
    item.size = file.size - offset;
  }
  // 长度异常或越界时视为无效截断 Box
  if (item.size < item.header || item.size > file.size - offset) return Box{.offset = offset};
  return item;
}

struct Sample {
  std::uint64_t offset = 0;
  std::uint64_t dts = 0;
  std::uint32_t size = 0;
  std::uint32_t duration = 0;
  std::int64_t cts = 0;
  bool sync = false;
};
struct Track {
  std::uint32_t id = 0;
  std::uint32_t timescale = 0;
  std::uint32_t default_duration = 0;
  std::uint32_t default_size = 0;
  std::uint32_t default_flags = 0;
  std::uint32_t description = 1;
  std::uint64_t next_dts = 0;
  std::uint64_t leading_empty = 0;
  std::uint64_t media_start = 0;
  std::optional<std::uint64_t> edit_duration;
  std::vector<Sample> samples;
};
struct Movie {
  Bytes init;
  std::uint64_t init_offset = 0;
  std::uint32_t timescale = 0;
  std::vector<Track> tracks;
};
auto rescale(std::uint64_t value, std::uint32_t to, std::uint32_t from) -> Result<std::uint64_t>;

// 解析 trak 下的编辑列表（edts/elst）：提取首帧前导空段与有效媒体时间范围
auto parse_edit(View trak, Track& track) -> Result<void> {
  auto list = children(trak);
  if (!list) return std::unexpected(list.error());
  for (const auto& item : *list) {
    if (item.type != tag('e', 'd', 't', 's')) continue;
    // 获取编辑列表 elst 载荷
    auto edits = find(content(trak, item), tag('e', 'l', 's', 't'));
    if (!edits || edits->size() < 8) return std::unexpected("Invalid MP4 edit list");
    const auto version = (*edits)[0];
    if (version > 1) return std::unexpected("Unsupported MP4 edit list version");
    const auto stride = version ? 20u : 12u;
    const auto count = u32(*edits, 4);
    if (count > 2 || std::uint64_t(count) * stride + 8 != edits->size())
      return std::unexpected("Unsupported MP4 edit list layout");
    // 遍历编辑项：提取前导空段和媒体有效播放区间
    for (std::uint32_t i = 0; i < count; ++i) {
      const auto at = 8 + i * stride;
      const auto duration = version ? u64(*edits, at) : u32(*edits, at);
      const auto media_time = version
                                  ? std::bit_cast<std::int64_t>(u64(*edits, at + 8))
                                  : std::int64_t(std::bit_cast<std::int32_t>(u32(*edits, at + 4)));
      if (u32(*edits, at + stride - 4) != 0x00010000)
        return std::unexpected("Unsupported MP4 edit playback rate");
      // media_time 为 -1 时记录为空白延时段
      if (media_time == -1 && i + 1 < count)
        track.leading_empty = duration;
      else if (media_time >= 0 && i + 1 == count) {
        // 记录媒体有效展示起点与时长
        track.media_start = std::uint64_t(media_time);
        if (duration) track.edit_duration = duration;
      } else
        return std::unexpected("Unsupported MP4 media edit");
    }
  }
  return {};
}

// 解析初始化的 moov 结构：提取时间基准、各轨初始参数、默认配置及统一时间戳精度
auto parse_movie(Bytes init, std::uint64_t offset) -> Result<Movie> {
  Movie movie{.init = std::move(init), .init_offset = offset};
  auto root = parse_box(movie.init, 0);
  if (!root) return std::unexpected(root.error());
  const auto payload = content(movie.init, *root);
  // 读取电影头 mvhd 获取全局初始时间刻度
  auto mvhd = find(payload, tag('m', 'v', 'h', 'd'));
  if (!mvhd || mvhd->size() < 20 || (*mvhd)[0] > 1) return std::unexpected("Invalid movie header");
  if ((*mvhd)[0] == 1 && mvhd->size() < 32) return std::unexpected("Truncated movie header");
  movie.timescale = u32(*mvhd, (*mvhd)[0] == 1 ? 20 : 12);
  if (!movie.timescale) return std::unexpected("Movie timescale is zero");
  auto list = children(payload);
  if (!list) return std::unexpected(list.error());
  // 遍历所有轨道容器 trak
  for (const auto& item : *list) {
    if (item.type != tag('t', 'r', 'a', 'k')) continue;
    const auto trak = content(payload, item);
    auto tkhd = find(trak, tag('t', 'k', 'h', 'd'));
    auto mdia = find(trak, tag('m', 'd', 'i', 'a'));
    if (!tkhd || !mdia || tkhd->size() < 24 || (*tkhd)[0] > 1)
      return std::unexpected("Invalid track header");
    const auto wide = (*tkhd)[0] == 1;
    if (wide && tkhd->size() < 36) return std::unexpected("Truncated track header");
    auto mdhd = find(*mdia, tag('m', 'd', 'h', 'd'));
    if (!mdhd || mdhd->size() < 20 || (*mdhd)[0] > 1 || ((*mdhd)[0] == 1 && mdhd->size() < 32))
      return std::unexpected("Invalid media header");
    auto minf = find(*mdia, tag('m', 'i', 'n', 'f'));
    if (!minf) return std::unexpected(minf.error());
    auto stbl = find(*minf, tag('s', 't', 'b', 'l'));
    if (!stbl) return std::unexpected(stbl.error());
    auto stsd = find(*stbl, tag('s', 't', 's', 'd'));
    if (!stsd || stsd->size() < 8 || u32(*stsd, 4) != 1)
      return std::unexpected("Expected one MP4 sample description");
    auto stsz = find(*stbl, tag('s', 't', 's', 'z'));
    if (!stsz || stsz->size() < 12)
      return std::unexpected("Missing initialization sample size table");
    // 本路径读取 MF 的空初始化 moov，不能忽略其他 muxer 已放在初始 moov 中的媒体。
    if (u32(*stsz, 8) != 0)
      return std::unexpected("Initialization moov already contains indexed media samples");
    // 构造轨道元数据并提取轨道 ID 与 timescale
    Track track{.id = u32(*tkhd, wide ? 20 : 12),
                .timescale = u32(*mdhd, (*mdhd)[0] == 1 ? 20 : 12)};
    if (!track.id || !track.timescale) return std::unexpected("Invalid track ID or timescale");
    for (const auto& previous : movie.tracks)
      if (previous.id == track.id) return std::unexpected("Duplicate MP4 track ID");
    // 解析本轨道的编辑列表配置
    auto edit = parse_edit(trak, track);
    if (!edit) return std::unexpected(edit.error());
    movie.tracks.push_back(std::move(track));
  }
  if (movie.tracks.empty()) return std::unexpected("No MP4 tracks");
  // 解析 mvex 容器以获取分片默认参数
  auto mvex = find(payload, tag('m', 'v', 'e', 'x'));
  if (!mvex) return std::unexpected("MP4 is not fragmented");
  auto defaults = children(*mvex);
  if (!defaults) return std::unexpected(defaults.error());
  // 遍历 trex 节点提取各轨道的默认时长、尺寸和标志
  for (const auto& item : *defaults) {
    if (item.type != tag('t', 'r', 'e', 'x')) continue;
    auto values = content(*mvex, item);
    if (values.size() != 24) return std::unexpected("Invalid trex defaults");
    auto track = std::ranges::find(movie.tracks, u32(values, 4), &Track::id);
    if (track == movie.tracks.end()) return std::unexpected("trex references unknown track");
    track->description = u32(values, 8);
    track->default_duration = u32(values, 12);
    track->default_size = u32(values, 16);
    track->default_flags = u32(values, 20);
  }
  // 普通 MP4 用 movie timescale 表达前导空段，提高精度以保留首帧的小数毫秒起点。
  const auto original_scale = movie.timescale;
  std::uint64_t common_scale = original_scale;
  auto finest_scale = original_scale;
  bool exact_scale = true;
  // 计算各轨时间刻度的最小公倍数以保证精度不丢
  for (const auto& track : movie.tracks) {
    finest_scale = std::max(finest_scale, track.timescale);
    if (exact_scale) {
      common_scale =
          common_scale / std::gcd(common_scale, std::uint64_t(track.timescale)) * track.timescale;
      exact_scale = common_scale <= UINT32_MAX;
    }
  }
  movie.timescale = exact_scale ? std::uint32_t(common_scale) : finest_scale;
  // 将各轨的前导空段和时长重新换算到统一的 movie timescale
  for (auto& track : movie.tracks) {
    auto leading = rescale(track.leading_empty, movie.timescale, original_scale);
    if (!leading) return std::unexpected(leading.error());
    track.leading_empty = *leading;
    if (track.edit_duration) {
      auto duration = rescale(*track.edit_duration, movie.timescale, original_scale);
      if (!duration) return std::unexpected(duration.error());
      track.edit_duration = *duration;
    }
  }
  return movie;
}

// 安全计算基准地址加带符号相对偏移，防止下溢或 64 位越界
auto add_offset(std::uint64_t base, std::int32_t delta) -> Result<std::uint64_t> {
  if (delta < 0) {
    const auto distance = std::uint64_t(-std::int64_t(delta));
    if (distance > base) return std::unexpected("Negative sample file position");
    return base - distance;
  }
  if (base > UINT64_MAX - std::uint32_t(delta)) return std::unexpected("Sample position overflow");
  return base + std::uint32_t(delta);
}

// 解析分片 moof 结构：提取各轨道的样本偏移量、DTS、CTS、时长及关键帧标记
auto parse_fragment(View moof, std::uint64_t file_offset, std::uint64_t file_size,
                    std::vector<Track>& tracks) -> Result<void> {
  auto root = parse_box(moof, 0);
  if (!root) return std::unexpected(root.error());
  auto payload = content(moof, *root);
  auto list = children(payload);
  if (!list) return std::unexpected(list.error());
  std::uint64_t implicit_base = file_offset;
  // 遍历 moof 中的所有 traf 轨道分片容器
  for (const auto& item : *list) {
    if (item.type != tag('t', 'r', 'a', 'f')) continue;
    const auto traf = content(payload, item);
    // 读取 tfhd 提取分片头与默认标志
    auto tfhd = find(traf, tag('t', 'f', 'h', 'd'));
    if (!tfhd || tfhd->size() < 8 || (*tfhd)[0] != 0)
      return std::unexpected("Invalid fragment track header");
    const auto flags = u32(*tfhd, 0) & 0xffffff;
    auto track = std::ranges::find(tracks, u32(*tfhd, 4), &Track::id);
    if (track == tracks.end()) return std::unexpected("Fragment references unknown track");
    // 计算当前 traf 的媒体数据基准偏移
    auto base = (flags & 0x020000) ? file_offset : implicit_base;
    std::size_t at = 8;
    if (flags & 1) {
      if (tfhd->size() - at < 8) return std::unexpected("Truncated base data offset");
      base = u64(*tfhd, at);
      at += 8;
    }
    // 读取 tfhd 中覆盖的默认属性
    auto description = track->description;
    auto duration = track->default_duration;
    auto size = track->default_size;
    auto sample_flags = track->default_flags;
    for (auto [mask, target] :
         {std::pair{2u, &description}, {8u, &duration}, {16u, &size}, {32u, &sample_flags}}) {
      if (!(flags & mask)) continue;
      if (tfhd->size() - at < 4) return std::unexpected("Truncated fragment defaults");
      *target = u32(*tfhd, at);
      at += 4;
    }
    // MF 当前每条轨道只有一个解码配置；不能把配置切换错误地合并为同一个 sample entry。
    if (description != 1) return std::unexpected("Multiple sample descriptions are unsupported");
    auto dts = track->next_dts;
    auto traf_boxes = children(traf);
    if (!traf_boxes) return std::unexpected(traf_boxes.error());
    // 读取 tfdt 获取显式指定的基准解码时间
    for (const auto& child : *traf_boxes) {
      if (child.type == tag('s', 'e', 'n', 'c') || child.type == tag('s', 'a', 'i', 'z') ||
          child.type == tag('s', 'a', 'i', 'o') || child.type == tag('s', 'b', 'g', 'p') ||
          child.type == tag('s', 'g', 'p', 'd'))
        return std::unexpected("Fragment sample encryption/groups require an explicit mapping");
      if (child.type != tag('t', 'f', 'd', 't')) continue;
      const auto time = content(traf, child);
      if (time.size() < 8 || time[0] > 1 || (time[0] == 1 && time.size() < 12))
        return std::unexpected("Invalid fragment decode time");
      dts = time[0] ? u64(time, 4) : u32(time, 4);
    }
    auto data_position = base;
    // 遍历所有 trun 样本运行记录
    for (const auto& child : *traf_boxes) {
      if (child.type != tag('t', 'r', 'u', 'n')) continue;
      const auto run = content(traf, child);
      if (run.size() < 8 || run[0] > 1) return std::unexpected("Invalid fragment run");
      const auto run_flags = u32(run, 0) & 0xffffff;
      const auto count = u32(run, 4);
      at = 8;
      // 若包含数据偏移差值则重新对齐起始位置
      if (run_flags & 1) {
        if (run.size() - at < 4) return std::unexpected("Truncated run data offset");
        auto position = add_offset(base, std::bit_cast<std::int32_t>(u32(run, at)));
        if (!position) return std::unexpected(position.error());
        data_position = *position;
        at += 4;
      }
      // 读取首样本标志覆盖（若存在）
      auto first_flags = sample_flags;
      if (run_flags & 4) {
        if (run.size() - at < 4) return std::unexpected("Truncated first sample flags");
        first_flags = u32(run, at);
        at += 4;
      }
      const auto stride = 4u * std::popcount(run_flags & 0x000f00u);
      if (std::uint64_t(count) * stride > run.size() - at)
        return std::unexpected("Truncated fragment sample table");
      // 即使所有样本属性由默认值给出，每个有效样本也至少占用一个媒体字节。
      if (count > file_size / ((run_flags & 0x200) ? 1u : std::max(size, 1u)))
        return std::unexpected("Sample run exceeds recording size");
      // 循环解析本批次内的各个样本参数
      for (std::uint32_t i = 0; i < count; ++i) {
        Sample sample{.offset = data_position, .dts = dts, .size = size, .duration = duration};
        auto current_flags = i == 0 ? first_flags : sample_flags;
        if (run_flags & 0x100) {
          sample.duration = u32(run, at);
          at += 4;
        }
        if (run_flags & 0x200) {
          sample.size = u32(run, at);
          at += 4;
        }
        if (run_flags & 0x400) {
          current_flags = u32(run, at);
          at += 4;
        }
        if (run_flags & 0x800) {
          const auto offset = u32(run, at);
          sample.cts =
              run[0] ? std::int64_t(std::bit_cast<std::int32_t>(offset)) : std::int64_t(offset);
          at += 4;
        }
        // 根据样本标志判定是否为关键帧（Sync Sample）
        sample.sync = !(current_flags & 0x10000);
        if (!sample.size || !sample.duration || data_position > UINT64_MAX - sample.size ||
            dts > INT64_MAX - std::uint64_t(sample.duration))
          return std::unexpected("Invalid fragment sample size or duration");
        // 校验 DTS 时间戳单调递增性
        if (!track->samples.empty() && dts <= track->samples.back().dts)
          return std::unexpected("Non-increasing MP4 decode timestamps");
        track->samples.push_back(sample);
        data_position += sample.size;
        dts += sample.duration;
      }
    }
    // 更新本轨道的推进位置供下个分片使用
    track->next_dts = dts;
    implicit_base = data_position;
  }
  return {};
}

// 高精度时间戳刻度缩放转换：避免中间乘法溢出
auto rescale(std::uint64_t value, std::uint32_t to, std::uint32_t from) -> Result<std::uint64_t> {
  const auto whole = value / from;
  const auto part = (value % from) * std::uint64_t(to) / from;
  if (whole > (UINT64_MAX - part) / to) return std::unexpected("MP4 duration overflow");
  return whole * to + part;
}

// 覆写 mvhd 或 tkhd 头部中的时长字段：若溢出 32 位则升级为 64 位宽头格式
auto duration_header(View payload, std::uint64_t duration, bool track_header) -> Result<Bytes> {
  const auto narrow_at = track_header ? 20u : 16u;
  const auto wide_at = track_header ? 28u : 24u;
  if (payload.size() < narrow_at + 4 || payload[0] > 1)
    return std::unexpected("Truncated MP4 duration header");
  Bytes out(payload.begin(), payload.end());
  // 原头本就是 64 位宽格式时直接覆写
  if (payload[0] == 1) {
    if (payload.size() < wide_at + 8) return std::unexpected("Truncated wide duration header");
    set64(out, wide_at, duration);
  } else if (duration <= UINT32_MAX) {
    // 32 位足以容纳时就地更新
    set32(out, narrow_at, std::uint32_t(duration));
  } else {
    // 32 位溢出时重构整个头部为 64 位版本 1 格式
    out.clear();
    append(out, payload.first(4));
    out[0] = 1;
    put64(out, u32(payload, 4));
    put64(out, u32(payload, 8));
    append(out, payload.subspan(12, narrow_at - 12));
    put64(out, duration);
    append(out, payload.subspan(narrow_at + 4));
  }
  return out;
}

struct TrackTiming {
  std::uint64_t media_duration = 0;
  std::uint64_t movie_duration = 0;
  std::uint64_t empty_duration = 0;
  std::uint64_t skip = 0;
  std::int64_t cts_shift = 0;
};

// 计算单个样本的解码时长（优先由后一帧 DTS 差值计算，末尾帧采用默认时长）
auto sample_delta(const Track& track, std::size_t index) -> std::uint64_t {
  const auto& sample = track.samples[index];
  return index + 1 < track.samples.size() ? track.samples[index + 1].dts - sample.dts
                                          : sample.duration;
}

// 将样本元数据序列化为完整 stbl 索引表：生成 stts、ctts、stss、stsc、stsz 与 co64
auto sample_tables(const Track& track, const TrackTiming& times) -> Result<Bytes> {
  const auto& samples = track.samples;
  if (samples.empty() || samples.size() > UINT32_MAX)
    return std::unexpected("Empty or oversized MP4 track");
  Bytes tables;
  Bytes stts(4, 0), ctts(4, 0), stss(4, 0), stsz(4, 0), co64(4, 0);
  std::vector<std::pair<std::uint32_t, std::uint32_t>> deltas;
  std::vector<std::pair<std::uint32_t, std::int64_t>> offsets;
  std::vector<std::uint32_t> sync;
  bool has_cts = false;
  bool signed_cts = false;
  put32(stsz, 0);
  put32(stsz, std::uint32_t(samples.size()));
  put32(co64, std::uint32_t(samples.size()));
  // 遍历所有样本，归纳相同步长与时间偏移并收集关键帧索引
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const auto& sample = samples[i];
    const auto delta = std::uint32_t(sample_delta(track, i));
    const auto cts = sample.cts + times.cts_shift;
    if (!deltas.empty() && deltas.back().second == delta)
      ++deltas.back().first;
    else
      deltas.emplace_back(1, delta);
    if (!offsets.empty() && offsets.back().second == cts)
      ++offsets.back().first;
    else
      offsets.emplace_back(1, cts);
    has_cts = has_cts || cts != 0;
    signed_cts = signed_cts || cts < 0;
    // 记录关键帧（从 1 开始编号）
    if (sample.sync) sync.push_back(std::uint32_t(i + 1));
    put32(stsz, sample.size);
    put64(co64, sample.offset);
  }
  // 写入解码时间表 stts
  put32(stts, std::uint32_t(deltas.size()));
  for (auto [count, delta] : deltas) {
    put32(stts, count);
    put32(stts, delta);
  }
  append(tables, box(tag('s', 't', 't', 's'), stts));
  // 存在非零 CTS 时构建组合时间偏移表 ctts
  if (has_cts) {
    ctts[0] = signed_cts ? 1 : 0;
    put32(ctts, std::uint32_t(offsets.size()));
    for (auto [count, offset] : offsets) {
      if (offset < INT32_MIN || offset > (signed_cts ? INT32_MAX : std::int64_t(UINT32_MAX)))
        return std::unexpected("Normalized composition offsets cannot share one ctts version");
      put32(ctts, count);
      put32(ctts, std::uint32_t(offset));
    }
    append(tables, box(tag('c', 't', 't', 's'), ctts));
  }
  // 写入关键帧索引表 stss
  put32(stss, std::uint32_t(sync.size()));
  for (auto index : sync) put32(stss, index);
  append(tables, box(tag('s', 't', 's', 's'), stss));
  // 每个样本作为一个 chunk，换取明确且与 MF 原有交织方式无关的偏移表。
  Bytes stsc(4, 0);
  for (auto value : {1u, 1u, 1u, 1u}) put32(stsc, value);
  append(tables, box(tag('s', 't', 's', 'c'), stsc));
  append(tables, box(tag('s', 't', 's', 'z'), stsz));
  append(tables, box(tag('c', 'o', '6', '4'), co64));
  return tables;
}

// 规划单轨道全局时序：计算媒体总时长、平移负向 CTS、修正 edit 映射并对齐轨道起点
auto timing(const Track& track, std::uint32_t movie_scale) -> Result<TrackTiming> {
  const auto& samples = track.samples;
  if (samples.empty() || samples.size() > UINT32_MAX)
    return std::unexpected("Empty or oversized MP4 track");
  TrackTiming value;
  const auto first = samples.front().dts;
  const auto& last = samples.back();
  value.media_duration = last.dts + last.duration - first;
  auto presentation_start = INT64_MAX;
  auto presentation_end = INT64_MIN;
  // 遍历所有样本统计展示时间范围（PTS = DTS + CTS）
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const auto& sample = samples[i];
    const auto delta = sample_delta(track, i);
    if (!delta || delta > UINT32_MAX || delta < sample.duration)
      return std::unexpected("Overlapping or unrepresentable sample timestamps");
    const auto dts = std::int64_t(sample.dts - first);
    if (sample.cts > 0 && dts > INT64_MAX - sample.cts)
      return std::unexpected("Track presentation timestamp overflow");
    const auto start = dts + sample.cts;
    if (start > INT64_MAX - std::int64_t(delta))
      return std::unexpected("Track presentation end overflow");
    presentation_start = std::min(presentation_start, start);
    presentation_end = std::max(presentation_end, start + std::int64_t(delta));
  }

  // DTS 从零累计；若最早 PTS 为负，统一平移 CTS，并由 edit 抵消坐标变化。
  // 只改变轨道内部表示，保留每条轨道原本在整段录像中的展示位置。
  value.cts_shift = presentation_start < 0 ? -presentation_start : 0;
  const auto media_start = std::int64_t(track.media_start) - std::int64_t(first);
  const auto begin = std::max(presentation_start, media_start);
  if (begin >= presentation_end) return std::unexpected("Edit skips the entire track");
  value.skip = begin < 0 ? std::uint64_t(value.cts_shift + begin)
                         : std::uint64_t(value.cts_shift) + std::uint64_t(begin);
  if (value.skip > INT64_MAX) return std::unexpected("Edit media time exceeds signed 64-bit range");

  // begin/end 均不早于 media_start；无符号差值也能准确表达跨零的非负距离。
  auto delay =
      rescale(std::uint64_t(begin) - std::uint64_t(media_start), movie_scale, track.timescale);
  auto end = rescale(std::uint64_t(presentation_end) - std::uint64_t(media_start), movie_scale,
                     track.timescale);
  if (!delay) return std::unexpected(delay.error());
  if (!end) return std::unexpected(end.error());
  if (track.edit_duration) *end = std::min(*end, *track.edit_duration);
  if (*end <= *delay) return std::unexpected("Edit has no presented media");
  if (*end > UINT64_MAX - track.leading_empty) return std::unexpected("Track duration overflow");
  // 计算最终在整个 movie 时间轴上的前导空段与时长
  value.empty_duration = track.leading_empty + *delay;
  value.movie_duration = track.leading_empty + *end;
  return value;
}

// 构造轨道的 edts/elst 容器：编码前导空段与媒体时间起始映射
auto edit_box(const TrackTiming& value) -> Bytes {
  Bytes elst{1, 0, 0, 0};
  put32(elst, value.empty_duration ? 2 : 1);
  // 有前导延迟时插入空段（media_time = -1）
  if (value.empty_duration) {
    put64(elst, value.empty_duration);
    put64(elst, UINT64_MAX);
    put32(elst, 0x10000);
  }
  // 插入媒体实际有效播放段映射
  put64(elst, value.movie_duration - value.empty_duration);
  put64(elst, value.skip);
  put32(elst, 0x10000);
  return box(tag('e', 'd', 't', 's'), box(tag('e', 'l', 's', 't'), elst));
}

// 递归重建 trak 节点树：剥离旧碎片索引、更新时长头、注入完整 sample 表及新 edit 表
auto rebuild(View payload, std::uint32_t parent, const Track& track, const TrackTiming& times)
    -> Result<Bytes> {
  auto list = children(payload);
  if (!list) return std::unexpected(list.error());
  Bytes out;
  // 遍历子节点并逐项更新
  for (const auto& item : *list) {
    const auto body = content(payload, item);
    const auto type = item.type;
    // 忽略旧的 edts，稍后由新生成的统一 edit 替代
    if (parent == tag('t', 'r', 'a', 'k') && type == tag('e', 'd', 't', 's')) continue;
    if (parent == tag('s', 't', 'b', 'l') && type != tag('s', 't', 's', 'd')) {
      // 旧的空索引、sample group 和辅助表不能与新的全局样本表混用。
      if (type == tag('s', 'a', 'i', 'z') || type == tag('s', 'a', 'i', 'o') ||
          type == tag('s', 'e', 'n', 'c') || type == tag('s', 'b', 'g', 'p') ||
          type == tag('s', 'g', 'p', 'd'))
        return std::unexpected("MP4 sample encryption/groups require an explicit mapping");
      continue;
    }
    // 更新 tkhd/mdhd 节点中的时长
    if (type == tag('t', 'k', 'h', 'd') || type == tag('m', 'd', 'h', 'd')) {
      const auto duration =
          type == tag('t', 'k', 'h', 'd') ? times.movie_duration : times.media_duration;
      auto updated = duration_header(body, duration, type == tag('t', 'k', 'h', 'd'));
      if (!updated) return std::unexpected(updated.error());
      append(out, box(type, *updated));
    } else if (type == tag('m', 'd', 'i', 'a') || type == tag('m', 'i', 'n', 'f') ||
               type == tag('s', 't', 'b', 'l')) {
      // 递归深入容器子树
      auto updated = rebuild(body, type, track, times);
      if (!updated) return std::unexpected(updated.error());
      append(out, box(type, *updated));
    } else {
      // 保留其他未经修改的原生节点
      append(out, payload.subspan(std::size_t(item.offset), std::size_t(item.size)));
    }
  }
  // 在 stbl 节点内追加重构后的全量样本索引表
  if (parent == tag('s', 't', 'b', 'l')) {
    auto tables = sample_tables(track, times);
    if (!tables) return std::unexpected(tables.error());
    append(out, *tables);
  } else if (parent == tag('t', 'r', 'a', 'k')) {
    // 在 trak 节点末尾追加重构后的 edts
    append(out, edit_box(times));
  }
  return out;
}

// 组装最终完整的总索引 moov：计算各轨时序、剥离 mvex 碎片声明、注入全局元数据
auto full_moov(const Movie& movie) -> Result<Bytes> {
  std::map<std::uint32_t, TrackTiming> timings;
  std::uint64_t duration = 0;
  // 遍历所有含有效样本的轨道计算时序，并取最长轨道作为电影全局时长
  for (const auto& track : movie.tracks) {
    if (track.samples.empty()) continue;
    auto value = timing(track, movie.timescale);
    if (!value) return std::unexpected(value.error());
    duration = std::max(duration, value->movie_duration);
    timings.emplace(track.id, std::move(*value));
  }
  auto root = parse_box(movie.init, 0);
  if (!root) return std::unexpected(root.error());
  const auto payload = content(movie.init, *root);
  auto list = children(payload);
  if (!list) return std::unexpected(list.error());
  Bytes result;
  // 遍历初始 moov 的各个直接子节点并重构
  for (const auto& item : *list) {
    const auto body = content(payload, item);
    // 丢弃不再需要的分片声明容器 mvex
    if (item.type == tag('m', 'v', 'e', 'x')) continue;
    if (item.type == tag('t', 'r', 'a', 'k')) {
      // 提取轨道 ID 并匹配重建
      auto tkhd = find(body, tag('t', 'k', 'h', 'd'));
      if (!tkhd) return std::unexpected(tkhd.error());
      const auto id = u32(*tkhd, (*tkhd)[0] == 1 ? 20 : 12);
      auto track = std::ranges::find(movie.tracks, id, &Track::id);
      if (track == movie.tracks.end()) return std::unexpected("Unknown track while rebuilding");
      if (track->samples.empty()) continue;
      auto updated = rebuild(body, item.type, *track, timings.at(id));
      if (!updated) return std::unexpected(updated.error());
      append(result, box(item.type, *updated));
    } else if (item.type == tag('m', 'v', 'h', 'd')) {
      // 更新全局 mvhd 时长与 timescale
      auto updated = duration_header(body, duration, false);
      if (!updated) return std::unexpected(updated.error());
      set32(*updated, (*updated)[0] == 1 ? 20 : 12, movie.timescale);
      append(result, box(item.type, *updated));
    } else {
      append(result, payload.subspan(std::size_t(item.offset), std::size_t(item.size)));
    }
  }
  return box(moov_tag, result);
}

// 事务化提交：追加完整索引到文件末尾 → 强刷磁盘 → 将初始头改写为大 mdat 覆盖碎片结构
// 只在本次调用内保存原头和长度。先持久化总索引，最后才替换文件头。
auto commit(File& file, std::uint64_t offset, View moov) -> Result<void> {
  const auto original_size = file.size;
  // 备份原初始 Box 头部，用于失败时回滚
  auto original_header = read(file, offset, 16);
  if (!original_header) return std::unexpected(original_header.error());
  // 定义回滚操作：还原头部字节、截断追加数据并恢复文件大小
  auto restore = [&]() -> Result<void> {
    auto restored = write(file, offset, *original_header);
    if (!restored) return restored;
    auto positioned = seek(file, original_size);
    if (!positioned) return positioned;
    if (!SetEndOfFile(file.handle.get()))
      return std::unexpected(io_error("Restore recording length"));
    file.size = original_size;
    return flush(file);
  };
  auto fail = [&](const std::string& error) -> Result<void> {
    auto restored = restore();
    return std::unexpected(error + (restored ? "" : "; restore: " + restored.error()));
  };
  // 步骤 1：将完整的全新 moov 索引追加写入到文件末尾
  auto appended = write(file, original_size, moov);
  if (!appended) return fail(appended.error());
  // 步骤 2：强制刷盘确保新索引数据落盘
  auto persisted = flush(file);
  if (!persisted) return fail(persisted.error());
  // 步骤 3：就地将原初始 moov 头部改写为包揽全量媒体的大 mdat 头部
  Bytes header;
  put32(header, 1);
  put32(header, mdat_tag);
  put64(header, original_size - offset);
  auto replaced = write(file, offset, header);
  if (!replaced) return fail(replaced.error());
  // 步骤 4：再次刷盘完成元数据最终提交
  persisted = flush(file);
  if (!persisted) return fail(persisted.error());
  return {};
}

}  // namespace

// 固化修复录制 MP4：扫描顶层 Box → 收集完整分片 → 重构全量索引 → 原地改写文件头
auto finalize(const std::filesystem::path& path) -> Result<void> {
  // 步骤 1：独占打开录制文件
  auto file = open_file(path);
  if (!file) return std::unexpected(file.error());
  std::vector<Box> moofs, mdats;
  std::optional<Movie> movie;
  bool incomplete_tail = false;
  bool regular_moov = false;
  std::uint64_t regular_samples = 0;
  // 步骤 2：顺序扫描文件中的所有顶层 Box
  for (std::uint64_t at = 0; at < file->size;) {
    auto item = top_box(*file, at);
    if (!item) return std::unexpected(item.error());
    // 发现未完成的截断尾部 Box 时记录标记并停止扫描
    if (!item->size) {
      incomplete_tail = true;
      break;
    }
    // 遇到 moov 节点时解析元数据，判断是否已固化或仍为分片模式
    if (item->type == moov_tag) {
      auto bytes = read(*file, item->offset, std::size_t(item->size));
      if (!bytes) return std::unexpected(bytes.error());
      auto root = parse_box(*bytes, 0);
      auto list = root ? children(content(*bytes, *root))
                       : Result<std::vector<Box>>(std::unexpected(root.error()));
      if (!list) return std::unexpected(list.error());
      // 检查是否包含分片声明 mvex
      const bool fragmented = std::ranges::any_of(
          *list, [](const Box& b) { return b.type == tag('m', 'v', 'e', 'x'); });
      if (!fragmented) {
        // 常规非分片 moov：累计已有样本数用于合法性判定
        regular_moov = true;
        const auto payload = content(*bytes, *root);
        for (const auto& track : *list) {
          if (track.type != tag('t', 'r', 'a', 'k')) continue;
          auto mdia = find(content(payload, track), tag('m', 'd', 'i', 'a'));
          if (!mdia) return std::unexpected(mdia.error());
          auto minf = find(*mdia, tag('m', 'i', 'n', 'f'));
          if (!minf) return std::unexpected(minf.error());
          auto stbl = find(*minf, tag('s', 't', 'b', 'l'));
          if (!stbl) return std::unexpected(stbl.error());
          auto stsz = find(*stbl, tag('s', 't', 's', 'z'));
          if (!stsz || stsz->size() < 12)
            return std::unexpected("Invalid regular MP4 sample table");
          regular_samples += u32(*stsz, 8);
        }
      } else {
        // 分片初始化 moov：提取初始元数据
        if (movie) return std::unexpected("Multiple initialization moov boxes");
        auto parsed = parse_movie(std::move(*bytes), at);
        if (!parsed) return std::unexpected(parsed.error());
        movie = std::move(*parsed);
      }
    } else if (item->type == tag('m', 'o', 'o', 'f'))
      moofs.push_back(*item);
    else if (item->type == mdat_tag)
      mdats.push_back(*item);
    at += item->size;
  }
  // 文件已是完整有效的常规 MP4 且未残留分片时直接返回
  if (regular_moov && !movie && !incomplete_tail && regular_samples && !mdats.empty()) return {};
  if (!movie || moofs.empty() || mdats.empty())
    return std::unexpected("No complete fragmented recording to finalize");
  if (regular_moov || movie->init.size() < 16)
    return std::unexpected("Unsupported Hybrid MP4 initialization layout");
  std::uint64_t sample_count = 0;
  // 步骤 3：逐个分片读取并校验样本物理分布
  for (std::size_t i = 0; i < moofs.size(); ++i) {
    auto bytes = read(*file, moofs[i].offset, std::size_t(moofs[i].size));
    if (!bytes) return std::unexpected(bytes.error());
    // 尾片段缺少完整 mdat 时，不把它的样本写入最终索引。
    std::vector<std::size_t> previous_sizes;
    for (const auto& track : movie->tracks) {
      previous_sizes.push_back(track.samples.size());
    }
    // 解析本分片的 sample 索引
    auto parsed = parse_fragment(*bytes, moofs[i].offset, file->size, movie->tracks);
    if (!parsed) return std::unexpected(parsed.error());
    bool valid = true;
    std::uint64_t added = 0;
    // 校验新增样本的物理偏移是否全部落在完整的 mdat 范围之内
    for (std::size_t t = 0; t < movie->tracks.size(); ++t) {
      const auto& samples = movie->tracks[t].samples;
      for (std::size_t s = previous_sizes[t]; s < samples.size(); ++s) {
        const auto& sample = samples[s];
        const auto media = std::ranges::upper_bound(mdats, sample.offset, {}, &Box::offset);
        if (media == mdats.begin()) {
          valid = false;
          break;
        }
        const auto& region = *std::prev(media);
        if (sample.offset < region.offset + region.header ||
            sample.offset > region.offset + region.size ||
            sample.size > region.offset + region.size - sample.offset) {
          valid = false;
          break;
        }
        ++added;
      }
    }
    // 样本不在合法媒体块中时进行容错处理
    if (!valid) {
      // 若中间分片损坏则报错；末尾截断分片则安全丢弃
      if (i + 1 != moofs.size())
        return std::unexpected("Fragment samples lie outside complete media boxes");
      for (std::size_t t = 0; t < movie->tracks.size(); ++t) {
        movie->tracks[t].samples.resize(previous_sizes[t]);
      }
      break;
    }
    sample_count += added;
  }
  if (!sample_count) return std::unexpected("Recording has no complete media samples");
  // 步骤 4：基于所有有效样本重构全量 moov 索引
  auto metadata = full_moov(*movie);
  if (!metadata) return std::unexpected(metadata.error());
  // 步骤 5：事务化提交：尾部追加新索引并就地改写文件头
  return commit(*file, movie->init_offset, *metadata);
}

}  // namespace utils::media::hybrid_mp4
