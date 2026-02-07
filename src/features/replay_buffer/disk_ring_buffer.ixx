module;

export module Features.ReplayBuffer.DiskRingBuffer;

import std;
import Utils.Media.RawEncoder;
import <mfapi.h>;
import <wil/com.h>;

export namespace Features::ReplayBuffer::DiskRingBuffer {

// 压缩帧元数据
struct FrameMetadata {
  std::int64_t file_offset;      // 在数据文件中的字节偏移
  std::int64_t timestamp_100ns;  // 时间戳（100ns 单位）
  std::uint32_t size;            // 帧数据大小（字节）
  std::int64_t duration_100ns;   // 帧持续时长（100ns 单位）
  bool is_keyframe;              // 是否为关键帧
  bool is_audio;                 // 是否为音频帧
};

// 硬盘环形缓冲上下文
struct DiskRingBufferContext {
  // 数据文件路径
  std::filesystem::path data_file_path;

  // 数据文件流（读写模式）
  mutable std::fstream data_file;

  // 帧索引（内存中，快速查找）
  std::deque<FrameMetadata> frame_index;

  // 编码器媒体类型（用于 mux）
  wil::com_ptr<IMFMediaType> video_media_type;
  wil::com_ptr<IMFMediaType> audio_media_type;

  // 视频 codec private data（SPS/PPS）
  std::vector<std::uint8_t> video_codec_data;

  // 写入状态
  std::int64_t write_position = 0;   // 当前写入位置
  std::int64_t file_size_limit = 0;  // 文件大小上限

  // 线程安全
  mutable std::mutex mutex;
};

// 初始化缓冲（传入编码器的媒体类型和 SPS/PPS）
auto initialize(DiskRingBufferContext& ctx, const std::filesystem::path& cache_dir,
                std::int64_t max_file_size, IMFMediaType* video_type, IMFMediaType* audio_type,
                const std::vector<std::uint8_t>& video_codec_data)
    -> std::expected<void, std::string>;

// 追加压缩帧数据（原始接口）
auto append_frame(DiskRingBufferContext& ctx, const BYTE* data, std::uint32_t size,
                  std::int64_t timestamp_100ns, std::int64_t duration_100ns, bool is_keyframe,
                  bool is_audio) -> std::expected<void, std::string>;

// 追加压缩帧（从 RawEncoder::EncodedFrame）
auto append_encoded_frame(DiskRingBufferContext& ctx,
                          const Utils::Media::RawEncoder::EncodedFrame& frame)
    -> std::expected<void, std::string>;

// 获取最近 N 秒的帧元数据（从最近的关键帧开始）
auto get_recent_frames(const DiskRingBufferContext& ctx, double duration_seconds)
    -> std::expected<std::vector<FrameMetadata>, std::string>;

// 读取单帧数据（返回数据缓冲区）
auto read_frame(const DiskRingBufferContext& ctx, const FrameMetadata& meta)
    -> std::expected<std::vector<std::uint8_t>, std::string>;

// 清理资源并删除缓存文件
auto cleanup(DiskRingBufferContext& ctx) -> void;

}  // namespace Features::ReplayBuffer::DiskRingBuffer
