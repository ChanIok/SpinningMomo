#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows/mfidl.hpp"
#include "vendor/windows/mfreadwrite.hpp"

namespace utils::media::sink_writer {

// 显式创建 MF 分片 MP4 sink，桌面编码与 ADB 压缩样本直通共用。
auto create_fragmented(const std::filesystem::path& path, IMFAttributes* attributes,
                       wil::com_ptr<IMFSinkWriter>& writer) -> std::expected<void, std::string>;

// finish=true 排空编码器；失败退出时仅关闭资源，保留已输出的片段。
auto close(wil::com_ptr<IMFSinkWriter>& writer, bool finish) -> std::expected<void, std::string>;

}  // namespace utils::media::sink_writer
