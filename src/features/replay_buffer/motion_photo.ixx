module;

export module Features.ReplayBuffer.MotionPhoto;

import std;

namespace Features::ReplayBuffer::MotionPhoto {

// 合成 Google Motion Photo 文件
// jpeg_path: 源 JPEG 图片路径
// mp4_path: 源 MP4 视频路径
// output_path: 输出 Motion Photo 文件路径
// presentation_timestamp_us: 截图时刻在视频中的位置（微秒）
export auto create_motion_photo(const std::filesystem::path& jpeg_path,
                                const std::filesystem::path& mp4_path,
                                const std::filesystem::path& output_path,
                                std::int64_t presentation_timestamp_us)
    -> std::expected<void, std::string>;

}  // namespace Features::ReplayBuffer::MotionPhoto
