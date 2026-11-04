#pragma once

#include "vendor/std.hpp"

namespace utils::media::hybrid_mp4 {

// MF 必须先关闭文件。追加完整索引并最后改头，音视频数据保持原位，不重编码、不改文件名。
// 重建 CTS 与 edit 的共同时间映射，保留轨道间起点差及已有 edit 的有效时长。
// 普通 I/O 失败会尝试还原当次修改；不提供跨进程提交恢复。
auto finalize(const std::filesystem::path& path) -> std::expected<void, std::string>;

}  // namespace utils::media::hybrid_mp4
