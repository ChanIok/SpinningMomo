#include "utils/media/sink_writer.hpp"

#include "vendor/std.hpp"

#include "vendor/windows/mfapi.hpp"
#include "vendor/windows/mferror.hpp"
#include "vendor/windows/mfidl.hpp"

namespace utils::media::sink_writer {

auto create_fragmented(const std::filesystem::path& path, IMFAttributes* attributes,
                       wil::com_ptr<IMFSinkWriter>& writer) -> std::expected<void, std::string> {
  wil::com_ptr<IMFByteStream> stream;
  auto hr = MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE,
                         path.c_str(), stream.put());
  if (FAILED(hr))
    return std::unexpected(std::format("Failed to create fragmented MP4 stream: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  // 显式创建分片 sink；URL 工厂的 FMPEG4 路径在 HEVC 协商时会拒绝相同的有效媒体类型。
  wil::com_ptr<IMFMediaSink> sink;
  hr = MFCreateFMPEG4MediaSink(stream.get(), nullptr, nullptr, sink.put());
  if (FAILED(hr))
    return std::unexpected(std::format("Failed to create fragmented MP4 sink: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  hr = MFCreateSinkWriterFromMediaSink(sink.get(), attributes, writer.put());
  if (FAILED(hr)) sink->Shutdown();
  if (FAILED(hr))
    return std::unexpected(std::format("Failed to create fragmented MP4 writer: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(hr)));
  return {};
}

auto close(wil::com_ptr<IMFSinkWriter>& writer, bool finish) -> std::expected<void, std::string> {
  if (!writer) return {};
  wil::com_ptr<IMFMediaSink> sink;
  const auto service_hr =
      writer->GetServiceForStream(MF_SINK_WRITER_MEDIASINK, GUID_NULL, IID_PPV_ARGS(sink.put()));
  const auto finalize_hr = finish ? writer->Finalize() : S_OK;
  writer.reset();
  const auto shutdown_hr = sink ? sink->Shutdown() : service_hr;
  sink.reset();
  if (FAILED(finalize_hr))
    return std::unexpected(std::format("Failed to finalize fragmented MP4: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(finalize_hr)));
  if (FAILED(shutdown_hr) && shutdown_hr != MF_E_SHUTDOWN)
    return std::unexpected(std::format("Failed to close fragmented MP4 sink: hr=0x{:08X}",
                                       static_cast<std::uint32_t>(shutdown_hr)));
  return {};
}

}  // namespace utils::media::sink_writer
