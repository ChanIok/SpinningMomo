#include "features/recording/mux.hpp"

#include "vendor/std.hpp"
#include "vendor/windows/mferror.hpp"
#include "vendor/windows/mfreadwrite.hpp"

#include "features/recording/state.hpp"
#include "features/recording/time.hpp"
#include "utils/logger/logger.hpp"
#include "utils/media/hybrid_mp4.hpp"
#include "utils/media/sink_writer.hpp"

namespace features::recording::mux {

#define MF_TRY(call)                                                                  \
  do {                                                                                \
    const auto hr_ = (call);                                                          \
    if (FAILED(hr_)) return std::unexpected(utils::media::mft::hr_error(#call, hr_)); \
  } while (false)

// 将压缩数据包提交到待封装队列，校验内存上限并记录告警水位。
auto enqueue(core::AppState& app_state, unsigned stream, utils::media::mft::Packet packet)
    -> std::expected<void, std::string> {
  auto& state = *app_state.recording->session;
  std::lock_guard lock(state.mutex);

  // 流水线已失败时直接拒绝入队
  if (state.failed.load()) {
    return std::unexpected("Recording pipeline failed");
  }

  // 达到内存硬上限时失败，不阻塞编码线程或丢弃已编码包。
  if (packet.bytes > k_max_packet_bytes - state.packet_bytes) {
    return std::unexpected(
        std::format("Recording compressed queue exceeded {}MiB: file output is congested",
                    k_max_packet_bytes / (1024 * 1024)));
  }

  // 记录内存占用并放入指定流的待写队列
  state.packet_bytes += packet.bytes;
  state.peak_packet_bytes = std::max(state.peak_packet_bytes, state.packet_bytes);
  state.streams[stream].packets.push_back(std::move(packet));
  if (state.packet_bytes >= k_packet_warning_bytes && !state.packet_warning_since_100ns) {
    state.packet_warning_since_100ns = time::query_qpc_100ns();
  }

  // 唤醒封装写盘线程
  state.changed.notify_all();
  return {};
}

// 封装工作线程主循环：创建分片 MP4 写入器 → 轮询并交错写入音视频压缩包 → 最终收尾与索引整理
auto run(core::AppState& app_state) -> std::expected<void, std::string> {
  auto& state = *app_state.recording->session;
  std::array<wil::com_ptr<IMFMediaType>, 2> types;

  // 等待视频和音频编码器完成首帧输出或类型协商
  {
    std::unique_lock lock(state.mutex);
    state.changed.wait(lock, [&] {
      return state.failed.load() || (state.streams[0].type && state.streams[1].ready) ||
             (state.streams[0].done && state.streams[1].done);
    });

    if (state.failed.load()) {
      return {};
    }

    for (UINT i = 0; i < 2; ++i) {
      types[i] = state.streams[i].type;
    }
  }

  // 视频是核心流，若未产生输出类型且有画面输入则报错
  if (!types[0]) {
    if (!state.video_submitted.load()) {
      return {};
    }
    return std::unexpected("Cannot mux: video encoder produced no format");
  }

  const auto path = state.config.output_path;
  wil::com_ptr<IMFSinkWriter> writer;

  // 异常退出时的安全兜底清理：非正常结束时只做 abort 关闭
  auto cleanup = wil::scope_exit([&] {
    auto closed = utils::media::sink_writer::close(writer, false);
    if (!closed) {
      Logger().warn("Recording mux cleanup: {}", closed.error());
    }
  });

  // 配置分片 MP4 写入器属性：强制禁用内部格式转换器，实现纯压缩包直通
  wil::com_ptr<IMFAttributes> attributes;
  MF_TRY(MFCreateAttributes(attributes.put(), 1));
  MF_TRY(attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE));

  auto created = utils::media::sink_writer::create_fragmented(path, attributes.get(), writer);
  if (!created) {
    return created;
  }
  state.output_created.store(true);

  // 依次向 SinkWriter 注册视频和音频流
  std::array<DWORD, 2> streams{};
  for (UINT i = 0; i < 2; ++i) {
    if (!types[i]) {
      continue;
    }
    MF_TRY(writer->AddStream(types[i].get(), &streams[i]));
    MF_TRY(writer->SetInputMediaType(streams[i], types[i].get(), nullptr));
  }

  // 开始写入会话
  MF_TRY(writer->BeginWriting());

  // 双重校验：检查 SinkWriter 内部链路是否插入了多余的 MFT 转换器
  wil::com_ptr<IMFSinkWriterEx> extended;
  MF_TRY(writer->QueryInterface(IID_PPV_ARGS(extended.put())));
  for (UINT i = 0; i < 2; ++i) {
    if (!types[i]) {
      continue;
    }
    const auto stream = streams[i];
    GUID category{};
    wil::com_ptr<IMFTransform> transform;
    const auto hr = extended->GetTransformForStream(stream, 0, &category, transform.put());
    if (SUCCEEDED(hr)) {
      return std::unexpected("Mux-only Sink Writer unexpectedly inserted an MFT");
    }
    if (hr != MF_E_INVALIDINDEX) {
      return std::unexpected(utils::media::mft::hr_error("Verify mux-only transform chain", hr));
    }
  }

  Logger().info("Recording mux: compressed pass-through, audio={}, no MFTs", bool(types[1]));

  std::array<std::optional<LONGLONG>, 2> last_dts;

  // 写入主循环：只要未发生致命失败就持续交错消费压缩包
  while (!state.failed.load()) {
    UINT chosen = 0;
    utils::media::mft::Packet packet;

    // 取包：音视频均有包时选 DTS 较小者交错写入；单路已完成则排空另一路
    {
      std::unique_lock lock(state.mutex);
      state.mux_stage.store("wait_packets");

      state.changed.wait(lock, [&] {
        if (state.failed.load()) {
          return true;
        }
        const auto& video = state.streams[0];
        const auto& audio = state.streams[1];
        return (!video.packets.empty() && (!audio.packets.empty() || audio.done)) ||
               (!audio.packets.empty() && video.done) || (video.done && audio.done);
      });

      if (state.failed.load()) {
        return {};
      }

      auto& video = state.streams[0];
      auto& audio = state.streams[1];

      // 两路队列均空且均已标记 done，说明全部编码数据已消费完毕
      if (video.packets.empty() && audio.packets.empty()) {
        break;
      }

      // 决定本次优先写入哪一路的数据包
      if (video.packets.empty()) {
        chosen = 1;
      } else if (!audio.packets.empty() && audio.packets.front().dts < video.packets.front().dts) {
        chosen = 1;
      }

      // 出队并扣除积压内存占用
      packet = std::move(state.streams[chosen].packets.front());
      state.streams[chosen].packets.pop_front();
      state.packet_bytes -= packet.bytes;
      // 出队时立即清除计时，避免两次状态巡检之间的回落被忽略。
      if (state.packet_bytes < k_packet_warning_bytes) {
        state.packet_warning_since_100ns = 0;
      }
    }

    // 校验 DTS 单调递增性，防止封装损坏
    if (last_dts[chosen] && packet.dts <= *last_dts[chosen]) {
      return std::unexpected("Non-increasing DTS at mux input");
    }
    last_dts[chosen] = packet.dts;

    // 写入压缩样本至分片 MP4
    state.mux_stage.store("WriteSample.compressed");
    const auto begin = time::query_qpc_100ns();
    const auto hr = writer->WriteSample(streams[chosen], packet.sample.get());
    const auto write_ms = (time::query_qpc_100ns() - begin) / 10000.0;

    // 监控磁盘写入阻塞时间
    if (write_ms > 100) {
      Logger().warn("Recording compressed write blocked for {:.3f}ms", write_ms);
    }
    if (FAILED(hr)) {
      return std::unexpected(utils::media::mft::hr_error("Mux compressed WriteSample", hr));
    }
  }

  if (state.failed.load()) {
    return {};
  }

  // 正常完工：调用 Finalize 写入分片结尾并关闭写入器
  state.mux_stage.store("Finalize");
  const auto begin = time::query_qpc_100ns();
  auto closed = utils::media::sink_writer::close(writer, true);
  if (!closed) {
    return closed;
  }
  Logger().info("Recording mux finalize: {:.3f}ms", (time::query_qpc_100ns() - begin) / 10000.0);

  // 原位重构 Hybrid MP4：将分片 MP4 索引整理到文件头部，兼顾录制防崩溃与播放秒开
  state.mux_stage.store("Hybrid MP4");
  const auto hybrid_begin = time::query_qpc_100ns();
  auto finalized = utils::media::hybrid_mp4::finalize(path);
  if (!finalized) {
    return finalized;
  }
  Logger().info("Recording Hybrid finalize: {:.3f}ms",
                (time::query_qpc_100ns() - hybrid_begin) / 10000.0);

  state.mux_stage.store("done");
  return {};
}

#undef MF_TRY

}  // namespace features::recording::mux
