export module Utils.Media.VideoAsset;

import std;
import Utils.Image;

export namespace Utils::Media::VideoAsset {

// 单次扫描/监听内对单个视频文件的解析结果；thumbnail 仅在传入 short_edge 时填充。
struct VideoAnalysis {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::string mime_type;
  std::optional<std::int64_t> duration_millis;
  std::optional<Utils::Image::WebPEncodedResult> thumbnail;
};

// 依赖进程内已 MFStartup；thumbnail_short_edge 为 nullopt 时跳过解码，仅填元数据。
export auto analyze_video_file(const std::filesystem::path& path,
                               std::optional<std::uint32_t> thumbnail_short_edge = std::nullopt)
    -> std::expected<VideoAnalysis, std::string>;

}  // namespace Utils::Media::VideoAsset
