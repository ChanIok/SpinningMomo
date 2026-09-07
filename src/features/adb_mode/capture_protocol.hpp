#pragma once

#include "vendor/std.hpp"

namespace features::adb_mode::capture_protocol {

constexpr std::uint32_t kMagic = 0x4D4F4D4Fu;  // "MOMO"，网络字节序写入。
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kHeaderSize = 28;
constexpr std::uint32_t kMaxPayloadSize = 128u * 1024u * 1024u;

enum class MessageType : std::uint16_t {
  Ready = 1,
  ScreenshotRequest = 2,
  ImageResponse = 3,
  Error = 4,
  Shutdown = 5,
  ShutdownAck = 6,
  StartRecord = 7,
  VideoConfig = 8,
  VideoSample = 9,
  StopRecord = 10,
  RecordFinished = 11,
};

struct Frame {
  MessageType type = MessageType::Error;
  std::uint32_t request_id = 0;
  std::uint32_t flags = 0;
  std::uint64_t timestamp = 0;
  std::vector<std::uint8_t> payload;
};

}  // namespace features::adb_mode::capture_protocol
