#include "features/recording/audio.hpp"

#include "vendor/std.hpp"
#include "vendor/windows/avrt.hpp"
#include "vendor/windows/mferror.hpp"

#include "features/recording/audio_negotiation.hpp"
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

// 拼接各候选编码器的失败原因，构成可诊断的错误信息
auto join_diagnostics(const std::vector<std::string>& failures) -> std::string {
  std::string joined;
  for (const auto& failure : failures) {
    if (!joined.empty()) {
      joined += "; ";
    }
    joined += failure;
  }
  return joined;
}

// 构建未压缩 PCM 输入媒体类型：与采集层统一协商的 48kHz / 16-bit / 2ch 对齐
auto pcm_input_type() -> std::expected<wil::com_ptr<IMFMediaType>, std::string> {
  wil::com_ptr<IMFMediaType> type;
  MF_TRY(MFCreateMediaType(type.put()));

  MF_TRY(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
  MF_TRY(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, k_audio_sample_rate));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                         k_audio_sample_rate * k_audio_bytes_per_frame));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, k_audio_bytes_per_frame));
  MF_TRY(type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));

  return type;
}

// 交给 MPEG-4 容器的是「容器侧类型」，与交给编码器 MFT 的类型属于两套契约，不能复用同一份。
// 微软 MPEG-4 File Sink 文档：AAC 的 MF_MT_USER_DATA 可选，缺省时按 AAC-LC 自行生成 stsd；
// SinkWriter 对多余属性敏感，因此这里只写文档要求的属性集（与 playground 探针验证过的一致）。
// 码率与 profile 取自编码器协商结果，不假设任何码率白名单。
auto sink_output_type(IMFMediaType* negotiated, std::uint32_t bytes_per_second)
    -> std::expected<wil::com_ptr<IMFMediaType>, std::string> {
  UINT32 profile = 0x29;
  (void)negotiated->GetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, &profile);

  wil::com_ptr<IMFMediaType> type;
  MF_TRY(MFCreateMediaType(type.put()));
  MF_TRY(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
  MF_TRY(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, k_audio_sample_rate));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
  MF_TRY(type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytes_per_second));
  MF_TRY(type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0));
  MF_TRY(type->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, profile));
  return type;
}

struct ConfiguredAac {
  wil::com_ptr<IMFMediaType> stream_type;  // 容器侧类型，交给封装线程 AddStream
  wil::com_ptr<IMFMediaType> configured;   // 编码器侧类型，供 mft::pull 回写动态格式变更
  std::uint32_t bitrate_bps = 0;           // 实际生效的码率
};

// 编码器侧输出类型来自 GetOutputAvailableType：选能实现目标码率的最低档，
// 超出上限时饱和到最高档；选中的档位若被拒则直接报错。
// 容器侧类型另行构造，见 sink_output_type。
auto configure_aac(IMFTransform* encoder, IMFMediaType* input, std::uint32_t target_bitrate_bps)
    -> std::expected<ConfiguredAac, std::string> {
  // 可用输出类型集合依赖输入类型，必须先设输入
  MF_TRY(encoder->SetInputType(0, input, 0));

  std::vector<wil::com_ptr<IMFMediaType>> types;
  std::vector<audio_negotiation::OutputCandidate> candidates;
  for (UINT32 index = 0;; ++index) {
    wil::com_ptr<IMFMediaType> type;
    const auto hr = encoder->GetOutputAvailableType(0, index, type.put());
    if (hr == MF_E_NO_MORE_TYPES) {
      break;
    }
    if (FAILED(hr)) {
      return std::unexpected(utils::media::mft::hr_error("GetOutputAvailableType", hr));
    }

    audio_negotiation::OutputCandidate candidate;
    // 码率为协商必需项；格式属性缺失时保留零值，表示编码器未声明该属性
    (void)type->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &candidate.bytes_per_second);
    (void)type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &candidate.sample_rate);
    (void)type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &candidate.channels);
    candidates.push_back(candidate);
    types.push_back(std::move(type));
  }

  const auto selected =
      audio_negotiation::select_candidate(candidates, target_bitrate_bps, k_audio_sample_rate, 2);
  if (!selected) {
    return std::unexpected(std::format("no AAC output type matches {}Hz/2ch PCM (available={})",
                                       k_audio_sample_rate, candidates.size()));
  }

  const auto index = *selected;
  const auto hr = encoder->SetOutputType(0, types[index].get(), 0);
  if (FAILED(hr)) {
    return std::unexpected(std::format("SetOutputType(AAC, {}B/s) rejected selected candidate: {}",
                                       candidates[index].bytes_per_second,
                                       utils::media::mft::hr_error("SetOutputType", hr)));
  }

  wil::com_ptr<IMFMediaType> negotiated;
  MF_TRY(encoder->GetOutputCurrentType(0, negotiated.put()));

  UINT32 bytes_per_second = candidates[index].bytes_per_second;
  (void)negotiated->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bytes_per_second);

  auto stream_type = sink_output_type(negotiated.get(), bytes_per_second);
  if (!stream_type) {
    return std::unexpected(stream_type.error());
  }

  // 本线程保留编码器侧完整类型，供 mft::pull 回写动态格式变更
  wil::com_ptr<IMFMediaType> configured;
  MF_TRY(MFCreateMediaType(configured.put()));
  MF_TRY(configured->CopyAllItems(negotiated.get()));

  return ConfiguredAac{
      .stream_type = std::move(*stream_type),
      .configured = std::move(configured),
      .bitrate_bps = bytes_per_second * 8,
  };
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

  // 枚举全部同步 AAC 编码器 MFT，逐个尝试协商，首个可用者胜出
  const MFT_REGISTER_TYPE_INFO in{MFMediaType_Audio, MFAudioFormat_PCM};
  const MFT_REGISTER_TYPE_INFO out{MFMediaType_Audio, MFAudioFormat_AAC};
  auto activations = utils::media::mft::enumerate(
      MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER, in, out);
  if (!activations) {
    return std::unexpected(activations.error());
  }

  auto input = pcm_input_type();
  if (!input) {
    return std::unexpected(input.error());
  }

  const auto target_bitrate = session.config.audio_bitrate;
  wil::com_ptr<IMFTransform> encoder;
  wil::com_ptr<IMFMediaType> output;
  wil::com_ptr<IMFMediaType> stream_type;
  std::string encoder_name;
  std::uint32_t effective_bitrate = 0;
  std::vector<std::string> failures;

  for (const auto& activation : *activations) {
    const auto identity = utils::media::mft::activation_identity(activation.get());

    wil::com_ptr<IMFTransform> candidate;
    const auto activated = activation->ActivateObject(IID_PPV_ARGS(candidate.put()));
    if (FAILED(activated)) {
      failures.push_back(std::format("{}: {}", identity,
                                     utils::media::mft::hr_error("ActivateObject", activated)));
      continue;
    }

    auto configured = configure_aac(candidate.get(), input->get(), target_bitrate);
    if (!configured) {
      failures.push_back(std::format("{}: {}", identity, configured.error()));
      MFShutdownObject(candidate.get());
      continue;
    }

    encoder = std::move(candidate);
    output = std::move(configured->configured);
    stream_type = std::move(configured->stream_type);
    effective_bitrate = configured->bitrate_bps;
    encoder_name = utils::media::mft::describe(activation.get());
    break;
  }

  if (!encoder) {
    return std::unexpected(std::format(
        "Audio encoder setup failed (target={}kbps, format={}Hz/2ch): no usable AAC MFT; "
        "try 192 kbps or disable audio recording; details=[{}]",
        target_bitrate / 1000, k_audio_sample_rate, join_diagnostics(failures)));
  }

  auto shutdown = wil::scope_exit([&] { MFShutdownObject(encoder.get()); });

  Logger().info("Recording audio encoder: {}, target={}kbps, effective={}kbps, mfts={}",
                encoder_name, target_bitrate / 1000, effective_bitrate / 1000, activations->size());

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
    session.streams[1].type = std::move(stream_type);
    session.streams[1].ready = true;
    session.changed.notify_all();
  }

  // 等待全局录制正式开始信号
  if (!wait_session_start(app_state)) {
    return {};
  }

  std::optional<LONGLONG> last_pts;

  // 辅助闭包：从 AAC MFT 拉取全部可读的压缩数据包并投递给封装队列
  auto collect = [&]() -> std::expected<void, std::string> {
    while (!session.failed.load()) {
      session.audio_stage.store("ProcessOutput");
      auto sample =
          utils::media::mft::pull(encoder.get(), utils::media::mft::PullMode::synchronous);
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
