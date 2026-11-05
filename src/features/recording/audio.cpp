#include "features/recording/audio.hpp"

#include "vendor/std.hpp"
#include "vendor/windows/avrt.hpp"
#include "vendor/windows/mferror.hpp"

#include "features/recording/mux.hpp"
#include "features/recording/recording.hpp"
#include "features/recording/state.hpp"
#include "features/recording/time.hpp"
#include "utils/logger/logger.hpp"
#include "utils/media/audio_capture.hpp"
#include "utils/media/mft.hpp"

namespace features::recording::audio {
namespace {

constexpr LONGLONG ticks_per_second = 10'000'000;
constexpr UINT block_frames = 480;  // 48kHz 下 10ms 对应的采样点数

// 给 WASAPI 正常交付抖动预留 40ms 缓冲延迟，避免短时网络或音频驱动抖动引起误填静音
constexpr LONGLONG delivery_delay = 400'000;

#define MF_TRY(call)                                                                  \
  do {                                                                                \
    const auto hr_ = (call);                                                          \
    if (FAILED(hr_)) return std::unexpected(utils::media::mft::hr_error(#call, hr_)); \
  } while (false)

// 构建音频媒体类型：区分未压缩 16-bit PCM 输入与 AAC 压缩输出
auto audio_type(bool compressed, UINT bitrate)
    -> std::expected<wil::com_ptr<IMFMediaType>, std::string> {
  wil::com_ptr<IMFMediaType> type;
  MF_TRY(MFCreateMediaType(type.put()));

  MF_TRY(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
  MF_TRY(type->SetGUID(MF_MT_SUBTYPE, compressed ? MFAudioFormat_AAC : MFAudioFormat_PCM));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, k_audio_sample_rate));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, compressed ? bitrate / 8 : 192000));

  if (compressed) {
    MF_TRY(type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0));
    MF_TRY(type->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29));
  } else {
    MF_TRY(type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4));
    MF_TRY(type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));
  }

  return type;
}

// 清空当前积压的所有 PCM 来源采样并重置时间锚点
auto discard_pcm(RecordingSession& session) -> void {
  session.audio_discarded_frames.fetch_add(session.pcm_frames);
  session.pcm.clear();
  session.pcm_frames = 0;
  session.audio_anchor_qpc_100ns = 0;
  session.audio_source_frames = 0;
}

// 接收 WASAPI 采集回调送来的原始 PCM 包：时间戳平滑对齐、异常重置与队列入库
auto receive_pcm(RecordingSession& session, const BYTE* bytes, UINT32 frames, UINT64 timestamp,
                 UINT64, DWORD flags) -> void {
  // 会话未处于运行态或已失败则忽略
  if (!session.started.load() || session.failed.load()) {
    return;
  }

  QueuedAudioPacket packet;
  packet.num_frames = frames;

  const auto duration = LONGLONG(frames) * ticks_per_second / k_audio_sample_rate;

  // 校验 WASAPI 时间戳有效性，驱动标记错误时用当前接收时刻倒推起算
  packet.timestamp_100ns = timestamp && !(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                               ? LONGLONG(timestamp)
                               : time::query_qpc_100ns() - duration;

  packet.data.resize(std::size_t(frames) * k_audio_bytes_per_frame);

  // 静音包直接保留全零，否则复制实际采样数据
  if (bytes && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
    std::memcpy(packet.data.data(), bytes, packet.data.size());
  }

  std::lock_guard lock(session.mutex);

  // 超过截止时间戳的新音频包直接舍弃
  const auto cutoff = session.stop_qpc_100ns.load();
  if (cutoff >= 0 && packet.timestamp_100ns >= cutoff) {
    return;
  }

  // 检测到设备流不连续时重置时间基准锚点
  if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
    session.audio_anchor_qpc_100ns = 0;
    session.audio_source_frames = 0;
    Logger().warn("Recording audio source reported a capture discontinuity");
  }

  // 来源 PCM 队列超过 1 秒积压上限时重置缓冲，避免占用过高内存
  if (session.pcm_frames + frames > k_max_pcm_frames) {
    Logger().warn("Recording audio source buffer reset: queued_frames={}", session.pcm_frames);
    discard_pcm(session);
  }

  // 计算连续采样的理论预期位置，抖动小于 70ms 则平滑对齐到预期时钟
  const auto expected = session.audio_anchor_qpc_100ns +
                        time::timestamp_for_index(session.audio_source_frames, k_audio_sample_rate);
  if (session.audio_anchor_qpc_100ns && std::llabs(packet.timestamp_100ns - expected) < 700'000) {
    packet.timestamp_100ns = expected;
  } else {
    // 首次采样或时钟漂移超过阈值时重新树立锚点
    session.audio_anchor_qpc_100ns = packet.timestamp_100ns;
    session.audio_source_frames = 0;
  }
  session.audio_source_frames += frames;

  // 异常巨大的单包抛弃
  if (frames > k_max_pcm_frames) {
    session.audio_discarded_frames.fetch_add(frames);
    return;
  }

  // 放入会话的 PCM 待编码队列
  session.pcm_frames += frames;
  session.pcm.push_back(std::move(packet));
}

// 组装连续的 PCM 输出块：消费队列数据、裁剪过时数据、在采样空缺处补静音
auto render_pcm(RecordingSession& session, LONGLONG first, UINT frames, BYTE* destination) -> void {
  // 先把整块输出内存初始化为静音（0）
  std::memset(destination, 0, std::size_t(frames) * k_audio_bytes_per_frame);

  std::lock_guard lock(session.mutex);
  const auto end = first + frames;
  auto cursor = first;

  while (cursor < end && !session.pcm.empty()) {
    auto& packet = session.pcm.front();
    const auto start = time::sample_index_at(packet.timestamp_100ns - session.start_qpc_100ns,
                                             k_audio_sample_rate);
    auto available = start + packet.offset_frames;

    // 丢弃已经落后于当前游标的过时采样（例如录制起点前的包前缀）
    if (available < cursor) {
      const auto skip =
          UINT(std::min<LONGLONG>(cursor - available, packet.num_frames - packet.offset_frames));
      packet.offset_frames += skip;
      session.pcm_frames -= skip;
      session.audio_discarded_frames.fetch_add(skip);
      available += skip;
    }

    // 整包已耗尽则移除
    if (packet.offset_frames == packet.num_frames) {
      session.pcm.pop_front();
      continue;
    }

    // 后续包全部在当前请求范围之后，提前跳出（空缺部分保持静音）
    if (available >= end) {
      break;
    }

    // 若当前可读采样超前于游标，中间空隙作为静音跳过
    if (available > cursor) {
      session.audio_silent_frames.fetch_add(available - cursor);
      cursor = available;
    }

    // 复制有效采样数据到输出目标
    const auto count =
        UINT(std::min<LONGLONG>(end - cursor, packet.num_frames - packet.offset_frames));
    std::memcpy(destination + std::size_t(cursor - first) * k_audio_bytes_per_frame,
                packet.data.data() + std::size_t(packet.offset_frames) * k_audio_bytes_per_frame,
                std::size_t(count) * k_audio_bytes_per_frame);

    packet.offset_frames += count;
    session.pcm_frames -= count;
    cursor += count;

    if (packet.offset_frames == packet.num_frames) {
      session.pcm.pop_front();
    }
  }

  // 游标与末尾之间的空隙计入静音统计
  session.audio_silent_frames.fetch_add(end - cursor);
}

}  // namespace

// 音频工作线程主循环：配置 WASAPI 采集 → 连续 PCM 时钟与静音填充 → AAC MFT 编码 → 交付封装队列
auto run(core::AppState& app_state) -> std::expected<void, std::string> {
  auto& session = *app_state.recording->session;

  // 提升音频线程调度优先级（MMCSS）
  DWORD task = 0;
  const auto mmcss = AvSetMmThreadCharacteristicsW(L"Audio", &task);
  auto scheduling = wil::scope_exit([&] {
    if (mmcss) {
      AvRevertMmThreadCharacteristics(mmcss);
    }
  });

  // 枚举并激活同步 AAC 编码器 MFT
  const MFT_REGISTER_TYPE_INFO in{MFMediaType_Audio, MFAudioFormat_PCM};
  const MFT_REGISTER_TYPE_INFO out{MFMediaType_Audio, MFAudioFormat_AAC};
  auto activation = utils::media::mft::enumerate(
      MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER, in, out);
  if (!activation) {
    return std::unexpected(activation.error());
  }

  wil::com_ptr<IMFTransform> encoder;
  MF_TRY((*activation)->ActivateObject(IID_PPV_ARGS(encoder.put())));
  auto shutdown = wil::scope_exit([&] { MFShutdownObject(encoder.get()); });

  // 配置编码器的输入 PCM 与输出 AAC 媒体类型
  auto input = audio_type(false, session.config.audio_bitrate);
  auto output = audio_type(true, session.config.audio_bitrate);
  if (!input) {
    return std::unexpected(input.error());
  }
  if (!output) {
    return std::unexpected(output.error());
  }

  MF_TRY(encoder->SetOutputType(0, output->get(), 0));
  MF_TRY(encoder->SetInputType(0, input->get(), 0));

  // 获取编码器最终协商确定的输出格式
  wil::com_ptr<IMFMediaType> negotiated;
  MF_TRY(encoder->GetOutputCurrentType(0, negotiated.put()));

  MF_TRY(encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0));
  MF_TRY(encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

  // 启动底层的 WASAPI 音频捕获器（针对系统声音或游戏进程声音）
  utils::media::audio_capture::AudioCaptureContext capture;
  auto stop_capture = wil::scope_exit([&] { utils::media::audio_capture::stop(capture); });

  DWORD process_id = 0;
  GetWindowThreadProcessId(session.target_window, &process_id);
  if (session.config.audio_source == utils::media::audio_capture::AudioSource::GameOnly &&
      !process_id) {
    return std::unexpected("Cannot determine game audio process");
  }

  auto capturing = utils::media::audio_capture::start(
      capture, session.config.audio_source, process_id,
      [&session](const BYTE* bytes, UINT32 frames, UINT64 timestamp, UINT64 position, DWORD flags) {
        receive_pcm(session, bytes, frames, timestamp, position, flags);
      },
      [&session](bool available, const std::string& reason) {
        {
          std::lock_guard lock(session.mutex);
          session.audio_available = available;

          // 已捕获的有效声音仍可播放；重连只重建后续来源时钟，不抹掉前缀
          session.audio_anchor_qpc_100ns = 0;
          session.audio_source_frames = 0;
        }

        Logger().info("Recording audio source: available={} {}", available, reason);
      });
  if (!capturing) {
    return std::unexpected(capturing.error());
  }

  // 发布音频流就绪标志，允许封装线程开始写盘
  {
    std::lock_guard lock(session.mutex);
    session.streams[1].type = std::move(negotiated);
    session.streams[1].ready = true;
    session.changed.notify_all();
  }

  Logger().info("Recording audio encoder: {}", utils::media::mft::describe(activation->get()));

  // 等待全局录制正式开始信号
  if (!wait_session_start(app_state)) {
    return {};
  }

  std::optional<LONGLONG> last_pts;

  // 辅助闭包：从 AAC MFT 拉取全部可读的压缩数据包并投递给封装队列
  auto collect = [&]() -> std::expected<void, std::string> {
    while (!session.failed.load()) {
      session.audio_stage.store("ProcessOutput");
      auto sample = utils::media::mft::pull(encoder.get(), output->get());
      if (!sample) {
        return std::unexpected(sample.error());
      }
      if (!*sample) {
        break;
      }

      auto packet = utils::media::mft::packet_from(std::move(**sample));
      if (!packet) {
        return std::unexpected(packet.error());
      }

      // AAC 必须保证单调递增且 PTS 等于 DTS
      if (packet->dts != packet->pts || (last_pts && packet->pts <= *last_pts)) {
        return std::unexpected("AAC timestamps are not increasing");
      }
      last_pts = packet->pts;

      auto queued = mux::enqueue(app_state, 1, std::move(*packet));
      if (!queued) {
        return queued;
      }
      ++session.audio_encoded;
    }
    return {};
  };

  LONGLONG cursor = 0;
  bool capture_stopped = false;

  // 音频推进主循环：按固定 10ms (480 采样点) 时间格向前推进
  while (!session.failed.load()) {
    auto stop = session.stop_qpc_100ns.load();

    // 收到停止信号后立即停止 WASAPI 硬件采集，不再生产新包
    if (stop >= 0 && !capture_stopped) {
      utils::media::audio_capture::stop(capture);
      capture_stopped = true;
    }

    // 计算整场录制截至该时刻应有的总采样数
    const auto total =
        stop < 0 ? std::numeric_limits<LONGLONG>::max()
                 : std::max<LONGLONG>(0, time::sample_index_at(stop - session.start_qpc_100ns,
                                                               k_audio_sample_rate));
    if (cursor >= total) {
      break;
    }

    const auto frames = UINT(std::min<LONGLONG>(block_frames, total - cursor));
    const auto deadline = session.start_qpc_100ns +
                          time::timestamp_for_index(cursor + frames, k_audio_sample_rate) +
                          delivery_delay;
    const auto wait = deadline - time::query_qpc_100ns();

    // 录制仍在进行且当前采样块尚未到达应交付时刻时，等待缓冲到达
    session.audio_stage.store("wait_samples");
    if (stop < 0 && wait > 0) {
      std::unique_lock lock(session.mutex);
      session.changed.wait_for(lock, std::chrono::nanoseconds(wait * 100), [&] {
        return session.failed.load() || session.stop_qpc_100ns.load() >= 0;
      });
      continue;
    }

    // 音频处理自身若持续落后 1 秒以上，判定为系统级严重卡死
    if (wait < -ticks_per_second) {
      return std::unexpected("Audio encoder fell behind by more than one second");
    }

    // 分配未压缩 PCM 输入 Sample 并填充连续音频数据
    wil::com_ptr<IMFSample> sample;
    wil::com_ptr<IMFMediaBuffer> buffer;
    MF_TRY(MFCreateSample(sample.put()));
    MF_TRY(MFCreateMemoryBuffer(frames * k_audio_bytes_per_frame, buffer.put()));

    BYTE* bytes = nullptr;
    MF_TRY(buffer->Lock(&bytes, nullptr, nullptr));
    render_pcm(session, cursor, frames, bytes);
    MF_TRY(buffer->Unlock());

    MF_TRY(buffer->SetCurrentLength(frames * k_audio_bytes_per_frame));
    MF_TRY(sample->AddBuffer(buffer.get()));

    // 计算该采样块的时间戳与持续时长
    const auto pts = time::timestamp_for_index(cursor, k_audio_sample_rate);
    const auto end = time::timestamp_for_index(cursor + frames, k_audio_sample_rate);
    MF_TRY(sample->SetSampleTime(pts));
    MF_TRY(sample->SetSampleDuration(end - pts));

    // 送入 AAC 编码器
    session.audio_stage.store("ProcessInput");
    MF_TRY(encoder->ProcessInput(0, sample.get(), 0));

    cursor += frames;

    // 拉取编码输出
    auto collected = collect();
    if (!collected) {
      return collected;
    }
  }

  if (session.failed.load()) {
    return {};
  }

  // 正常完工排空：发送 EndOfStream 和 Drain 命令排空剩余 AAC 输出
  const auto began = time::query_qpc_100ns();
  session.audio_stage.store("drain");
  MF_TRY(encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0));
  MF_TRY(encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0));

  auto collected = collect();
  if (!collected) {
    return collected;
  }

  Logger().info("Recording AAC drain: {:.3f}ms", (time::query_qpc_100ns() - began) / 10000.0);
  session.audio_stage.store("done");
  return {};
}

#undef MF_TRY

}  // namespace features::recording::audio
