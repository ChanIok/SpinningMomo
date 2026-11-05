#include "utils/media/sink_writer.hpp"

#include "vendor/std.hpp"

#include "vendor/windows/mfapi.hpp"
#include "vendor/windows/mferror.hpp"
#include "vendor/windows/mfidl.hpp"

namespace utils::media::sink_writer {

// 创建分片 MP4 写入器：创建字节流 → 构造 FMPEG4 MediaSink → 包装为 IMFSinkWriter
auto create_fragmented(const std::filesystem::path& path, IMFAttributes* attributes,
                       wil::com_ptr<IMFSinkWriter>& writer) -> std::expected<void, std::string> {
  // 创建目标输出文件的可写字节流（若已存在同名文件则报错，不静默覆盖）
  wil::com_ptr<IMFByteStream> stream;
  auto hr = MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_FAIL_IF_EXIST, MF_FILEFLAGS_NONE,
                         path.c_str(), stream.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create fragmented MP4 stream: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }

  // 显式创建 FMPEG4 Media Sink（支持边录边写分片 MP4，遭遇断电或异常中止时已写数据依然可播）
  wil::com_ptr<IMFMediaSink> sink;
  hr = MFCreateFMPEG4MediaSink(stream.get(), nullptr, nullptr, sink.put());
  if (FAILED(hr)) {
    return std::unexpected(std::format("Failed to create fragmented MP4 sink: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }

  // 根据 MediaSink 与属性配置包装生成 SinkWriter
  hr = MFCreateSinkWriterFromMediaSink(sink.get(), attributes, writer.put());
  if (FAILED(hr)) {
    sink->Shutdown();
    return std::unexpected(std::format("Failed to create fragmented MP4 writer: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  }

  return {};
}

// 关闭写入器：finish=true 时执行 Finalize 正常封装，否则仅安全释放与 Shutdown
auto close(wil::com_ptr<IMFSinkWriter>& writer, bool finish) -> std::expected<void, std::string> {
  if (!writer) {
    return {};
  }

  // 获取底层 MediaSink 对象指针以便后续显式关闭
  wil::com_ptr<IMFMediaSink> sink;
  const auto service_hr =
      writer->GetServiceForStream(MF_SINK_WRITER_MEDIASINK, GUID_NULL, IID_PPV_ARGS(sink.put()));

  // 正常完工时调用 Finalize 写入 moov/mfra 等关键分片索引尾部信息
  const auto finalize_hr = finish ? writer->Finalize() : S_OK;

  // 释放 SinkWriter 句柄
  writer.reset();

  // 显式对底层 MediaSink 执行 Shutdown 清理内部资源
  const auto shutdown_hr = sink ? sink->Shutdown() : service_hr;
  sink.reset();

  if (FAILED(finalize_hr)) {
    return std::unexpected(std::format("Failed to finalize fragmented MP4: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(finalize_hr)));
  }

  if (FAILED(shutdown_hr) && shutdown_hr != MF_E_SHUTDOWN) {
    return std::unexpected(std::format("Failed to close fragmented MP4 sink: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(shutdown_hr)));
  }

  return {};
}

}  // namespace utils::media::sink_writer
