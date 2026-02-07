module;

#include <mfidl.h>

module Features.ReplayBuffer.DiskRingBuffer;

import std;
import Utils.Logger;
import Utils.Media.RawEncoder;
import <mfapi.h>;
import <wil/com.h>;

namespace Features::ReplayBuffer::DiskRingBuffer {

// 环形覆盖：删除旧帧以腾出空间（内部函数）
auto trim_old_frames(DiskRingBufferContext& ctx) -> void {
  // 删除最旧的帧，但保证至少保留一个关键帧
  if (ctx.frame_index.empty()) {
    return;
  }

  // 统计有多少关键帧
  size_t keyframe_count = 0;
  for (const auto& frame : ctx.frame_index) {
    if (frame.is_keyframe && !frame.is_audio) {
      keyframe_count++;
    }
  }

  // 删除最旧的帧，直到释放足够空间或只剩一个关键帧
  while (ctx.frame_index.size() > 1) {
    const auto& oldest = ctx.frame_index.front();

    // 如果是关键帧且只剩一个关键帧，不删除
    if (oldest.is_keyframe && !oldest.is_audio && keyframe_count <= 1) {
      break;
    }

    // 删除这一帧
    if (oldest.is_keyframe && !oldest.is_audio) {
      keyframe_count--;
    }

    // 从索引中移除（不需要实际删除文件数据，会被覆盖）
    ctx.frame_index.pop_front();

    // 释放了足够空间就停止
    if (!ctx.frame_index.empty()) {
      std::int64_t oldest_offset = ctx.frame_index.front().file_offset;
      if (ctx.write_position < oldest_offset) {
        // 写指针已经回绕，可以重用前面的空间
        ctx.write_position = 0;
        break;
      }
    }
  }
}

auto initialize(DiskRingBufferContext& ctx, const std::filesystem::path& cache_dir,
                std::int64_t max_file_size, IMFMediaType* video_type, IMFMediaType* audio_type,
                const std::vector<std::uint8_t>& video_codec_data)
    -> std::expected<void, std::string> {
  std::lock_guard lock(ctx.mutex);

  // 1. 确保缓存目录存在
  try {
    std::filesystem::create_directories(cache_dir);
  } catch (const std::exception& e) {
    return std::unexpected("Failed to create cache directory: " + std::string(e.what()));
  }

  // 2. 设置数据文件路径
  ctx.data_file_path = cache_dir / "buffer.dat";

  // 3. 打开数据文件（读写模式，如果存在则清空）
  ctx.data_file.open(ctx.data_file_path,
                     std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
  if (!ctx.data_file.is_open()) {
    return std::unexpected("Failed to open data file: " + ctx.data_file_path.string());
  }

  // 4. 保存媒体类型（深拷贝）
  if (video_type) {
    MFCreateMediaType(ctx.video_media_type.put());
    video_type->CopyAllItems(ctx.video_media_type.get());
  }
  if (audio_type) {
    MFCreateMediaType(ctx.audio_media_type.put());
    audio_type->CopyAllItems(ctx.audio_media_type.get());
  }

  // 5. 保存 codec private data
  ctx.video_codec_data = video_codec_data;

  // 6. 初始化状态
  ctx.file_size_limit = max_file_size;
  ctx.write_position = 0;
  ctx.frame_index.clear();

  Logger().info("DiskRingBuffer initialized: file_size_limit={} MB", max_file_size / 1024 / 1024);
  return {};
}

auto append_frame(DiskRingBufferContext& ctx, const BYTE* data, std::uint32_t size,
                  std::int64_t timestamp_100ns, std::int64_t duration_100ns, bool is_keyframe,
                  bool is_audio) -> std::expected<void, std::string> {
  if (!data || size == 0) {
    return std::unexpected("Invalid frame data");
  }

  std::lock_guard lock(ctx.mutex);

  if (!ctx.data_file.is_open()) {
    return std::unexpected("Data file not initialized");
  }

  // 1. 检查是否需要环形覆盖
  while (ctx.write_position + size > ctx.file_size_limit && !ctx.frame_index.empty()) {
    trim_old_frames(ctx);
  }

  // 2. 如果覆盖后仍然超出限制，说明单帧过大
  if (ctx.write_position + size > ctx.file_size_limit) {
    return std::unexpected("Frame size exceeds buffer limit");
  }

  // 3. 写入数据
  ctx.data_file.seekp(ctx.write_position);
  ctx.data_file.write(reinterpret_cast<const char*>(data), size);
  if (!ctx.data_file.good()) {
    return std::unexpected("Failed to write frame data to disk");
  }
  // 4. 记录元数据
  FrameMetadata meta;
  meta.file_offset = ctx.write_position;
  meta.timestamp_100ns = timestamp_100ns;
  meta.size = size;
  meta.duration_100ns = duration_100ns;
  meta.is_keyframe = is_keyframe;
  meta.is_audio = is_audio;
  ctx.frame_index.push_back(meta);

  // 5. 更新写入位置
  ctx.write_position += size;

  return {};
}

auto append_encoded_frame(DiskRingBufferContext& ctx,
                          const Utils::Media::RawEncoder::EncodedFrame& frame)
    -> std::expected<void, std::string> {
  return append_frame(ctx, frame.data.data(), static_cast<std::uint32_t>(frame.data.size()),
                      frame.timestamp_100ns, frame.duration_100ns, frame.is_keyframe,
                      frame.is_audio);
}

auto get_recent_frames(const DiskRingBufferContext& ctx, double duration_seconds)
    -> std::expected<std::vector<FrameMetadata>, std::string> {
  std::lock_guard lock(ctx.mutex);

  if (ctx.frame_index.empty()) {
    return std::unexpected("No frames in buffer");
  }

  std::vector<FrameMetadata> result;
  std::int64_t target_duration_100ns = static_cast<std::int64_t>(duration_seconds * 10'000'000);

  // 从最后一帧开始往前累加时长
  std::int64_t accumulated_duration = 0;
  size_t start_index = ctx.frame_index.size();

  for (size_t i = ctx.frame_index.size(); i > 0; --i) {
    size_t idx = i - 1;
    const auto& frame = ctx.frame_index[idx];

    accumulated_duration += frame.duration_100ns;
    start_index = idx;

    if (accumulated_duration >= target_duration_100ns) {
      break;
    }
  }

  // 从 start_index 往前找最近的关键帧
  size_t keyframe_index = start_index;
  for (size_t i = start_index; i > 0; --i) {
    size_t idx = i - 1;
    if (ctx.frame_index[idx].is_keyframe && !ctx.frame_index[idx].is_audio) {
      keyframe_index = idx;
      break;
    }
  }

  // 从关键帧开始收集所有帧
  for (size_t i = keyframe_index; i < ctx.frame_index.size(); ++i) {
    result.push_back(ctx.frame_index[i]);
  }

  Logger().debug("get_recent_frames: collected {} frames from index {} (keyframe at {})",
                 result.size(), start_index, keyframe_index);
  return result;
}

auto read_frame(const DiskRingBufferContext& ctx, const FrameMetadata& meta)
    -> std::expected<std::vector<std::uint8_t>, std::string> {
  std::lock_guard lock(ctx.mutex);

  if (!ctx.data_file.is_open()) {
    return std::unexpected("Data file not open");
  }

  // 分配缓冲区
  std::vector<std::uint8_t> out_buffer(meta.size);

  // 定位并读取
  ctx.data_file.seekg(meta.file_offset);
  ctx.data_file.read(reinterpret_cast<char*>(out_buffer.data()), meta.size);

  if (!ctx.data_file.good()) {
    return std::unexpected("Failed to read frame data from disk");
  }

  return out_buffer;
}

auto read_frames_bulk(const DiskRingBufferContext& ctx, const std::vector<FrameMetadata>& frames)
    -> std::expected<std::vector<std::vector<std::uint8_t>>, std::string> {
  std::lock_guard lock(ctx.mutex);

  if (!ctx.data_file.is_open()) {
    return std::unexpected("Data file not open");
  }

  std::vector<std::vector<std::uint8_t>> result;
  result.reserve(frames.size());

  for (const auto& meta : frames) {
    std::vector<std::uint8_t> buffer(meta.size);
    ctx.data_file.seekg(meta.file_offset);
    ctx.data_file.read(reinterpret_cast<char*>(buffer.data()), meta.size);

    if (!ctx.data_file.good()) {
      // 清除错误状态并继续
      ctx.data_file.clear();
      Logger().warn("Failed to read frame at offset {}", meta.file_offset);
      result.emplace_back();  // 空数据占位
    } else {
      result.push_back(std::move(buffer));
    }
  }

  return result;
}

auto read_frames_unlocked(const std::filesystem::path& data_file_path,
                          const std::vector<FrameMetadata>& frames)
    -> std::expected<std::vector<std::vector<std::uint8_t>>, std::string> {
  // 使用独立文件句柄读取，完全不需要获取 mutex
  // Windows 允许多个线程同时以只读模式打开同一个文件
  std::ifstream reader(data_file_path, std::ios::binary);
  if (!reader.is_open()) {
    return std::unexpected("Failed to open data file for reading: " + data_file_path.string());
  }

  std::vector<std::vector<std::uint8_t>> result;
  result.reserve(frames.size());

  for (const auto& meta : frames) {
    std::vector<std::uint8_t> buffer(meta.size);
    reader.seekg(meta.file_offset);
    reader.read(reinterpret_cast<char*>(buffer.data()), meta.size);

    if (!reader.good()) {
      reader.clear();
      Logger().warn("Failed to read frame at offset {} (unlocked)", meta.file_offset);
      result.emplace_back();  // 空数据占位
    } else {
      result.push_back(std::move(buffer));
    }
  }

  return result;
}

auto cleanup(DiskRingBufferContext& ctx) -> void {
  std::lock_guard lock(ctx.mutex);

  // 关闭文件
  if (ctx.data_file.is_open()) {
    ctx.data_file.close();
  }

  // 删除缓存文件
  if (!ctx.data_file_path.empty()) {
    std::error_code ec;
    std::filesystem::remove(ctx.data_file_path, ec);
    if (ec) {
      Logger().warn("Failed to remove cache file: {}", ctx.data_file_path.string());
    }
  }

  // 清理状态
  ctx.frame_index.clear();
  ctx.write_position = 0;
  ctx.video_media_type = nullptr;
  ctx.audio_media_type = nullptr;
  ctx.video_codec_data.clear();
}

}  // namespace Features::ReplayBuffer::DiskRingBuffer
