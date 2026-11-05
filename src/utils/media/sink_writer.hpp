#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows/mfidl.hpp"
#include "vendor/windows/mfreadwrite.hpp"

namespace utils::media::sink_writer {

// 创建分片 MP4 写入器：创建字节流 → 构造 FMPEG4 MediaSink → 包装为 IMFSinkWriter
auto create_fragmented(const std::filesystem::path& path, IMFAttributes* attributes,
                       wil::com_ptr<IMFSinkWriter>& writer) -> std::expected<void, std::string>;

// 关闭写入器：finish=true 时执行 Finalize 正常封装，否则仅安全释放与 Shutdown
auto close(wil::com_ptr<IMFSinkWriter>& writer, bool finish) -> std::expected<void, std::string>;

}  // namespace utils::media::sink_writer
