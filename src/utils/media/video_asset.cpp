module;

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wil/com.h>

module Utils.Media.VideoAsset;

import std;
import Utils.File.Mime;
import Utils.Image;

namespace Utils::Media::VideoAsset {

// MF 时长为 100ns 单位；封面时间点取「约 10%
// 时长」并夹在下面两常量之间，减少片头黑场又避免拖到过久才解码。
constexpr std::int64_t kHundredNanosecondsPerMillisecond = 10'000;
constexpr std::int64_t kMinThumbnailTimestampHns = 2'000'000;
constexpr std::int64_t kMaxThumbnailTimestampHns = 30'000'000;

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

auto calculate_thumbnail_timestamp_hns(std::optional<std::int64_t> duration_millis)
    -> std::int64_t {
  if (!duration_millis.has_value() || duration_millis.value() <= 0) {
    return 0;
  }

  auto duration_hns = duration_millis.value() * kHundredNanosecondsPerMillisecond;
  auto target = duration_hns / 10;
  // 上限不超过「最后一帧之前」，避免 seek 到 EOF 导致读不到样本。
  return std::clamp(
      target, kMinThumbnailTimestampHns,
      std::max(duration_hns - kHundredNanosecondsPerMillisecond, kMinThumbnailTimestampHns));
}

auto seek_source_reader(IMFSourceReader* reader, std::int64_t position_hns)
    -> std::expected<void, std::string> {
  PROPVARIANT position;
  PropVariantInit(&position);

  auto init_hr = InitPropVariantFromInt64(position_hns, &position);
  if (FAILED(init_hr)) {
    PropVariantClear(&position);
    return std::unexpected(format_hresult(init_hr, "Failed to build seek position"));
  }

  HRESULT hr = reader->SetCurrentPosition(GUID_NULL, position);
  PropVariantClear(&position);
  if (FAILED(hr)) {
    return std::unexpected(format_hresult(hr, "Failed to seek source reader"));
  }

  return {};
}

auto read_thumbnail_bitmap_data(IMFSourceReader* reader, std::uint32_t width, std::uint32_t height)
    -> std::expected<Utils::Image::BGRABitmapData, std::string> {
  constexpr DWORD kReadFlags = 0;
  // Seek 后前几帧可能是无效/空样本，多试几次再放弃。
  constexpr int kMaxReadAttempts = 120;

  for (int attempt = 0; attempt < kMaxReadAttempts; ++attempt) {
    DWORD actual_stream_index = 0;
    DWORD stream_flags = 0;
    LONGLONG timestamp = 0;
    wil::com_ptr<IMFSample> sample;

    HRESULT hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, kReadFlags,
                                    &actual_stream_index, &stream_flags, &timestamp, sample.put());
    if (FAILED(hr)) {
      return std::unexpected(format_hresult(hr, "Failed to read video sample"));
    }

    if ((stream_flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
      break;
    }

    if (!sample) {
      continue;
    }

    wil::com_ptr<IMFMediaBuffer> media_buffer;
    hr = sample->ConvertToContiguousBuffer(media_buffer.put());
    if (FAILED(hr) || !media_buffer) {
      return std::unexpected(format_hresult(hr, "Failed to get video buffer"));
    }

    BYTE* buffer_ptr = nullptr;
    DWORD max_length = 0;
    DWORD current_length = 0;
    hr = media_buffer->Lock(&buffer_ptr, &max_length, &current_length);
    if (FAILED(hr)) {
      return std::unexpected(format_hresult(hr, "Failed to lock video buffer"));
    }

    auto unlock_guard = std::unique_ptr<void, std::function<void(void*)>>(
        media_buffer.get(), [&media_buffer](void*) { media_buffer->Unlock(); });

    if (!buffer_ptr || current_length == 0) {
      continue;
    }

    std::uint32_t stride = width * 4;
    std::uint64_t expected_size = static_cast<std::uint64_t>(stride) * height;
    if (current_length < expected_size) {
      return std::unexpected("Video sample is smaller than expected RGB32 frame size");
    }

    Utils::Image::BGRABitmapData result;
    result.width = width;
    result.height = height;
    result.stride = stride;
    result.pixels.assign(buffer_ptr, buffer_ptr + expected_size);

    // MFVideoFormat_RGB32 的 X 通道未必有意义，统一填不透明 alpha，避免后续 WIC/WebP 出现意外透明。
    for (std::size_t i = 3; i < result.pixels.size(); i += 4) {
      result.pixels[i] = 255;
    }

    return result;
  }

  return std::unexpected("Failed to decode a video frame for thumbnail generation");
}

auto analyze_video_file(const std::filesystem::path& path,
                        std::optional<std::uint32_t> thumbnail_short_edge)
    -> std::expected<VideoAnalysis, std::string> {
  if (!std::filesystem::exists(path)) {
    return std::unexpected("Video file does not exist: " + path.string());
  }

  // 分析可能在 worker 线程调用；与已以别种模式初始化的 COM 线程共存时返回
  // RPC_E_CHANGED_MODE，此时不得 CoUninitialize。
  HRESULT coinit_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool need_uninitialize = SUCCEEDED(coinit_hr);
  if (FAILED(coinit_hr) && coinit_hr != RPC_E_CHANGED_MODE) {
    return std::unexpected(format_hresult(coinit_hr, "Failed to initialize COM"));
  }

  auto uninitialize_guard =
      std::unique_ptr<void, std::function<void(void*)>>(nullptr, [need_uninitialize](void*) {
        if (need_uninitialize) {
          CoUninitialize();
        }
      });

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

  auto output_result = configure_rgb32_output(reader.get());
  if (!output_result) {
    return std::unexpected(output_result.error());
  }

  auto seek_result =
      seek_source_reader(reader.get(), calculate_thumbnail_timestamp_hns(duration_millis));
  if (!seek_result) {
    return std::unexpected(seek_result.error());
  }

  auto bitmap_result = read_thumbnail_bitmap_data(reader.get(), width, height);
  if (!bitmap_result) {
    return std::unexpected(bitmap_result.error());
  }

  auto wic_factory_result = Utils::Image::get_thread_wic_factory();
  if (!wic_factory_result) {
    return std::unexpected("Failed to initialize WIC factory for video thumbnail: " +
                           wic_factory_result.error());
  }

  Utils::Image::WebPEncodeOptions webp_options;
  webp_options.quality = 90.0f;

  auto thumbnail_result = Utils::Image::generate_webp_thumbnail_from_bgra(
      wic_factory_result->get(), bitmap_result.value(), thumbnail_short_edge.value(), webp_options);
  if (!thumbnail_result) {
    return std::unexpected("Failed to encode video thumbnail: " + thumbnail_result.error());
  }

  result.thumbnail = std::move(thumbnail_result.value());
  return result;
}

}  // namespace Utils::Media::VideoAsset
