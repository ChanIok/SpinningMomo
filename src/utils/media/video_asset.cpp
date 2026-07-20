module;

#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

module Utils.Media.VideoAsset;

import std;
import Utils.File.Mime;
import Utils.Image;
import Utils.Logger;
import <mfapi.h>;
import <mferror.h>;
import <wil/com.h>;

namespace Utils::Media::VideoAsset {

// Media Foundation 使用 100ns 作为时长单位。
constexpr std::int64_t kHundredNanosecondsPerMillisecond = 10'000;
constexpr std::uint32_t kBgraBytesPerPixel = 4;

// Media Foundation 返回的视频帧并不总是“画面宽高 = 内存宽高”。
// 这里把“解码 surface 的布局”和“真正可见的画面区域”拆开保存，后续就只按这个结构取像素。
struct VideoFrameLayout {
  std::uint32_t surface_width = 0;
  std::uint32_t surface_height = 0;
  std::int32_t stride = 0;
  std::uint32_t visible_x = 0;
  std::uint32_t visible_y = 0;
  std::uint32_t visible_width = 0;
  std::uint32_t visible_height = 0;
};

auto format_hresult(HRESULT hr, const std::string& context) -> std::string {
  char* message_buffer = nullptr;
  auto size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, static_cast<DWORD>(hr), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

  std::string message;
  if (size > 0 && message_buffer) {
    message = std::format("{} (HRESULT: 0x{:08X}): {}", context, static_cast<unsigned int>(hr),
                          message_buffer);
    LocalFree(message_buffer);
  } else {
    message = std::format("{} (HRESULT: 0x{:08X})", context, static_cast<unsigned int>(hr));
  }

  return message;
}

auto get_propvariant_int64(const PROPVARIANT& value) -> std::optional<std::int64_t> {
  if (value.vt == VT_I8) {
    return value.hVal.QuadPart;
  }

  if (value.vt == VT_UI8) {
    return static_cast<std::int64_t>(value.uhVal.QuadPart);
  }

  return std::nullopt;
}

auto get_video_frame_size(IMFSourceReader* reader)
    -> std::expected<std::pair<std::uint32_t, std::uint32_t>, std::string> {
  wil::com_ptr<IMFMediaType> media_type;
  HRESULT hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, media_type.put());
  // 尚未 SetCurrentMediaType 时 Current 可能失败，回退到 native 类型取 MF_MT_FRAME_SIZE。
  if (FAILED(hr) || !media_type) {
    hr = reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, media_type.put());
  }

  if (FAILED(hr) || !media_type) {
    return std::unexpected(format_hresult(hr, "Failed to get video media type"));
  }

  UINT32 width = 0;
  UINT32 height = 0;
  hr = MFGetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, &width, &height);
  if (FAILED(hr) || width == 0 || height == 0) {
    return std::unexpected(format_hresult(hr, "Failed to get video frame size"));
  }

  return std::make_pair(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
}

auto try_get_media_type_frame_size(IMFMediaType* media_type)
    -> std::optional<std::pair<std::uint32_t, std::uint32_t>> {
  if (!media_type) {
    return std::nullopt;
  }

  UINT32 width = 0;
  UINT32 height = 0;
  auto hr = MFGetAttributeSize(media_type, MF_MT_FRAME_SIZE, &width, &height);
  if (FAILED(hr) || width == 0 || height == 0) {
    return std::nullopt;
  }

  return std::make_pair(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
}

auto try_get_media_type_default_stride(IMFMediaType* media_type) -> std::optional<std::int32_t> {
  if (!media_type) {
    return std::nullopt;
  }

  UINT32 stride = 0;
  auto hr = media_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  return static_cast<std::int32_t>(stride);
}

auto try_get_media_type_video_area(IMFMediaType* media_type, REFGUID key)
    -> std::optional<MFVideoArea> {
  if (!media_type) {
    return std::nullopt;
  }

  UINT32 blob_size = 0;
  auto hr = media_type->GetBlobSize(key, &blob_size);
  if (FAILED(hr) || blob_size != sizeof(MFVideoArea)) {
    return std::nullopt;
  }

  MFVideoArea area{};
  hr = media_type->GetBlob(key, reinterpret_cast<UINT8*>(&area), sizeof(area), nullptr);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  return area;
}

auto resolve_video_area_offset(const MFOffset& offset, const char* axis_name)
    -> std::expected<std::uint32_t, std::string> {
  if (offset.fract != 0 || offset.value < 0) {
    return std::unexpected(std::format("Unsupported non-integer {} aperture offset", axis_name));
  }

  return static_cast<std::uint32_t>(offset.value);
}

auto resolve_video_frame_layout(IMFMediaType* media_type)
    -> std::expected<VideoFrameLayout, std::string> {
  if (!media_type) {
    return std::unexpected("Video media type is null");
  }

  auto frame_size = try_get_media_type_frame_size(media_type);
  if (!frame_size) {
    return std::unexpected("Video media type does not expose MF_MT_FRAME_SIZE");
  }

  auto default_stride = try_get_media_type_default_stride(media_type);
  if (!default_stride.has_value()) {
    return std::unexpected("Video media type does not expose MF_MT_DEFAULT_STRIDE");
  }

  if (default_stride.value() <= 0) {
    return std::unexpected("Video media type exposes a non-positive default stride");
  }

  // 这里要区分两套尺寸：
  // 1. surface_*：解码器实际输出的内存表面大小，往往会按块对齐。
  // 2. visible_*：真正应该拿来做缩略图的可见区域。
  // 某些视频会出现「surface 比画面大一点」的情况；如果直接按 visible 宽高线性读整块
  // buffer，就会把每行末尾的填充像素误当成下一行开头，从而出现撕裂。
  VideoFrameLayout layout{
      .surface_width = frame_size->first,
      .surface_height = frame_size->second,
      .stride = default_stride.value(),
      .visible_x = 0,
      .visible_y = 0,
      .visible_width = frame_size->first,
      .visible_height = frame_size->second,
  };

  auto minimum_display_aperture =
      try_get_media_type_video_area(media_type, MF_MT_MINIMUM_DISPLAY_APERTURE);
  if (!minimum_display_aperture.has_value()) {
    return layout;
  }

  auto visible_x_result =
      resolve_video_area_offset(minimum_display_aperture->OffsetX, "horizontal");
  if (!visible_x_result) {
    return std::unexpected(visible_x_result.error());
  }

  auto visible_y_result = resolve_video_area_offset(minimum_display_aperture->OffsetY, "vertical");
  if (!visible_y_result) {
    return std::unexpected(visible_y_result.error());
  }

  layout.visible_x = visible_x_result.value();
  layout.visible_y = visible_y_result.value();
  layout.visible_width = minimum_display_aperture->Area.cx;
  layout.visible_height = minimum_display_aperture->Area.cy;

  if (layout.visible_width == 0 || layout.visible_height == 0) {
    return std::unexpected("Video media type exposes an empty minimum display aperture");
  }

  if (layout.visible_x + layout.visible_width > layout.surface_width ||
      layout.visible_y + layout.visible_height > layout.surface_height) {
    return std::unexpected("Video minimum display aperture exceeds decoded surface bounds");
  }

  return layout;
}

auto copy_bitmap_data_from_linear_buffer(const BYTE* buffer_start, std::uint32_t buffer_length,
                                         const VideoFrameLayout& layout)
    -> std::expected<Utils::Image::BGRABitmapData, std::string> {
  if (!buffer_start || buffer_length == 0) {
    return std::unexpected("Video buffer is empty");
  }

  if (layout.stride <= 0) {
    return std::unexpected("Video buffer stride must be positive");
  }

  auto stride = static_cast<std::uint64_t>(layout.stride);
  auto visible_row_bytes = static_cast<std::uint64_t>(layout.visible_width) * kBgraBytesPerPixel;
  auto row_offset = static_cast<std::uint64_t>(layout.visible_x) * kBgraBytesPerPixel;
  if (row_offset + visible_row_bytes > stride) {
    return std::unexpected("Video aperture row exceeds decoded surface stride");
  }

  auto last_row_offset =
      static_cast<std::uint64_t>(layout.visible_y + layout.visible_height - 1) * stride;
  auto required_bytes = last_row_offset + row_offset + visible_row_bytes;
  if (required_bytes > buffer_length) {
    return std::unexpected("Video buffer is smaller than the decoded surface layout requires");
  }

  auto pixel_bytes = visible_row_bytes * layout.visible_height;
  Utils::Image::BGRABitmapData result;
  result.width = layout.visible_width;
  result.height = layout.visible_height;
  result.stride = static_cast<std::uint32_t>(visible_row_bytes);
  result.pixels.resize(static_cast<std::size_t>(pixel_bytes));

  // 这里只拷贝可见区域；surface 右侧/底部的对齐填充会被自然跳过。
  for (std::uint32_t y = 0; y < layout.visible_height; ++y) {
    auto source_offset = (static_cast<std::uint64_t>(layout.visible_y + y) * stride) + row_offset;
    std::memcpy(result.pixels.data() + static_cast<std::size_t>(y) * result.stride,
                buffer_start + source_offset, result.stride);
  }

  for (std::size_t i = 3; i < result.pixels.size(); i += kBgraBytesPerPixel) {
    result.pixels[i] = 255;
  }

  return result;
}

auto create_source_reader(const std::filesystem::path& path)
    -> std::expected<wil::com_ptr<IMFSourceReader>, std::string> {
  wil::com_ptr<IMFAttributes> attributes;
  HRESULT hr = MFCreateAttributes(attributes.put(), 2);
  if (FAILED(hr)) {
    return std::unexpected(format_hresult(hr, "Failed to create source reader attributes"));
  }

  // 允许解码器做色彩空间/尺寸处理，便于统一输出 RGB32 供 WIC 走缩略图管线。
  attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
  attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);

  wil::com_ptr<IMFSourceReader> reader;
  hr = MFCreateSourceReaderFromURL(path.c_str(), attributes.get(), reader.put());
  if (FAILED(hr) || !reader) {
    return std::unexpected(format_hresult(hr, "Failed to create source reader"));
  }

  return reader;
}

auto configure_rgb32_output(IMFSourceReader* reader) -> std::expected<void, std::string> {
  wil::com_ptr<IMFMediaType> output_type;
  HRESULT hr = MFCreateMediaType(output_type.put());
  if (FAILED(hr)) {
    return std::unexpected(format_hresult(hr, "Failed to create RGB32 media type"));
  }

  output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

  hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output_type.get());
  if (FAILED(hr)) {
    return std::unexpected(format_hresult(hr, "Failed to configure RGB32 video output"));
  }

  hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
  if (FAILED(hr)) {
    return std::unexpected(format_hresult(hr, "Failed to select video stream"));
  }

  return {};
}

auto read_current_video_frame_layout(IMFSourceReader* reader)
    -> std::expected<VideoFrameLayout, std::string> {
  wil::com_ptr<IMFMediaType> media_type;
  auto hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, media_type.put());
  if (FAILED(hr) || !media_type) {
    return std::unexpected(format_hresult(hr, "Failed to get current video media type"));
  }

  return resolve_video_frame_layout(media_type.get());
}

// 从视频起点读取第一张 RGB32 画面。
auto read_first_thumbnail_bgra_frame(IMFSourceReader* reader)
    -> std::expected<Utils::Image::BGRABitmapData, std::string> {
  DWORD stream_flags = 0;
  wil::com_ptr<IMFSample> sample;
  auto hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &stream_flags,
                               nullptr, sample.put());
  if (FAILED(hr)) {
    return std::unexpected(format_hresult(hr, "Failed to read first video sample"));
  }
  if (!sample) {
    return std::unexpected("Video source did not return a first frame");
  }

  auto layout_result = read_current_video_frame_layout(reader);
  if (!layout_result) {
    return std::unexpected(layout_result.error());
  }

  wil::com_ptr<IMFMediaBuffer> media_buffer;
  hr = sample->GetBufferByIndex(0, media_buffer.put());
  if (FAILED(hr) || !media_buffer) {
    return std::unexpected(format_hresult(hr, "Failed to get first video frame buffer"));
  }

  BYTE* buffer_start = nullptr;
  DWORD current_length = 0;
  hr = media_buffer->Lock(&buffer_start, nullptr, &current_length);
  if (FAILED(hr)) {
    return std::unexpected(format_hresult(hr, "Failed to lock first video frame buffer"));
  }
  auto unlock_guard = wil::scope_exit([&media_buffer] { media_buffer->Unlock(); });

  if (!buffer_start || current_length == 0) {
    return std::unexpected("First video frame buffer is empty");
  }

  return copy_bitmap_data_from_linear_buffer(buffer_start, current_length, layout_result.value());
}

// 读取视频元数据，并按需抽取单帧编码为 WebP 缩略图
auto analyze_video_file(const std::filesystem::path& path,
                        std::optional<std::uint32_t> thumbnail_short_edge)
    -> std::expected<VideoAnalysis, std::string> {
  if (!std::filesystem::exists(path)) {
    return std::unexpected("Video file does not exist: " + path.string());
  }

  auto com_init = wil::CoInitializeEx(COINIT_MULTITHREADED);

  auto reader_result = create_source_reader(path);
  if (!reader_result) {
    return std::unexpected(reader_result.error());
  }
  auto reader = std::move(reader_result.value());

  auto frame_size_result = get_video_frame_size(reader.get());
  if (!frame_size_result) {
    return std::unexpected(frame_size_result.error());
  }
  auto [width, height] = frame_size_result.value();

  std::optional<std::int64_t> duration_millis;
  PROPVARIANT duration_value;
  PropVariantInit(&duration_value);
  if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION,
                                                 &duration_value))) {
    if (auto duration_hns = get_propvariant_int64(duration_value); duration_hns.has_value()) {
      duration_millis = duration_hns.value() / kHundredNanosecondsPerMillisecond;
    }
  }
  PropVariantClear(&duration_value);

  VideoAnalysis result{
      .width = width,
      .height = height,
      .mime_type = Utils::File::Mime::get_mime_type(path),
      .duration_millis = duration_millis,
      .thumbnail = std::nullopt,
  };

  // 不传 short_edge 时只做元数据，避免扫描「仅索引、不生成缩略图」场景下的解码开销。
  if (!thumbnail_short_edge.has_value()) {
    return result;
  }

  // 要求 reader 输出 RGB32，后续直接交给 WIC 生成缩略图。
  auto output_result = configure_rgb32_output(reader.get());
  if (!output_result) {
    return std::unexpected(output_result.error());
  }

  auto bitmap_result = read_first_thumbnail_bgra_frame(reader.get());
  if (!bitmap_result) {
    Logger().warn("Video analysis failed while decoding thumbnail bitmap. path='{}', error={}",
                  path.string(), bitmap_result.error());
    return std::unexpected(bitmap_result.error());
  }

  auto wic_factory_result = Utils::Image::get_thread_wic_factory();
  if (!wic_factory_result) {
    auto error =
        "Failed to initialize WIC factory for video thumbnail: " + wic_factory_result.error();
    Logger().warn("Video analysis failed while creating WIC factory. path='{}', error={}",
                  path.string(), error);
    return std::unexpected(error);
  }

  Utils::Image::WebPEncodeOptions webp_options;
  webp_options.quality = 80.0f;

  // 把 BGRA 位图缩放并编码成 WebP，供图库缩略图直接使用。
  auto thumbnail_result = Utils::Image::generate_webp_thumbnail_from_bgra(
      wic_factory_result->get(), bitmap_result.value(), thumbnail_short_edge.value(), webp_options);
  if (!thumbnail_result) {
    auto error = "Failed to encode video thumbnail: " + thumbnail_result.error();
    Logger().warn("Video analysis failed while encoding thumbnail. path='{}', error={}",
                  path.string(), error);
    return std::unexpected(error);
  }

  result.thumbnail = std::move(thumbnail_result.value());
  return result;
}

}  // namespace Utils::Media::VideoAsset
