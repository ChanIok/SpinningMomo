module;

#include <xxhash.h>

export module Vendor.XXHash;

import std;

namespace Vendor::XXHash {

constexpr std::size_t kReadBufferSize = 1024 * 1024;

export struct StreamRange {
  std::uint64_t offset = 0;
  std::size_t size = 0;
};

using StatePtr = std::unique_ptr<XXH3_state_t, decltype(&XXH3_freeState)>;

// 创建并初始化一次 XXH3 流式会话
auto create_state() -> std::expected<StatePtr, std::string> {
  auto state = StatePtr(XXH3_createState(), &XXH3_freeState);
  if (!state) {
    return std::unexpected("Failed to create XXH3 state");
  }

  if (XXH3_64bits_reset(state.get()) != XXH_OK) {
    return std::unexpected("Failed to reset XXH3 state");
  }

  return state;
}

// 将一段内存追加到当前 XXH3 会话
auto update_state(XXH3_state_t* state, const void* data, std::size_t size)
    -> std::expected<void, std::string> {
  if (size == 0) {
    return {};
  }

  if (XXH3_64bits_update(state, data, size) != XXH_OK) {
    return std::unexpected("Failed to update XXH3 state");
  }

  return {};
}

// 输出当前 XXH3 会话的十六进制摘要
auto digest_state(const XXH3_state_t* state) -> std::string {
  return std::format("{:016x}", XXH3_64bits_digest(state));
}

// 分块读取输入流并计算 XXH3 哈希，在块边界响应停止且不按输入总大小占用内存
export auto hash_stream_to_hex(std::istream& stream, std::stop_token stop_token)
    -> std::expected<std::string, std::string> {
  // 每次只保留 1 MiB 输入，限制单个哈希任务的内存上限
  std::vector<char> buffer(kReadBufferSize);

  // 将 XXH3 状态绑定到官方释放函数，确保所有提前返回都能清理资源
  auto state_result = create_state();
  if (!state_result) {
    return std::unexpected(state_result.error());
  }
  auto state = std::move(state_result.value());

  bool has_data = false;

  for (;;) {
    // 停止后不再发起下一次同步读取，退出延迟最多受当前读取块影响
    if (stop_token.stop_requested()) {
      return std::unexpected("Hash calculation cancelled");
    }

    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto bytes_read = stream.gcount();

    // 最后一轮即使遇到 EOF，也要先提交已经读到的剩余数据
    if (bytes_read > 0) {
      has_data = true;
      auto update_result =
          update_state(state.get(), buffer.data(), static_cast<std::size_t>(bytes_read));
      if (!update_result) {
        return std::unexpected(update_result.error());
      }
    }

    // badbit 表示底层读取失败，不能把已读到的部分内容当成完整文件
    if (stream.bad()) {
      return std::unexpected("Input stream read failed");
    }

    // eofbit 表示输入已经正常读完，可以结束流式计算
    if (stream.eof()) {
      break;
    }

    // 非 EOF 的 failbit 同样属于读取失败
    if (stream.fail()) {
      return std::unexpected("Input stream read failed");
    }
  }

  // 保持现有业务语义：空文件不生成可用的媒体哈希
  if (!has_data) {
    return std::unexpected("Input stream is empty");
  }

  // digest 与原来一次性 XXH3_64bits 的结果保持一致
  return digest_state(state.get());
}

// 依次读取指定区间，并把固定元数据与采样内容合并为一个 XXH3 指纹
export auto hash_stream_ranges_to_hex(std::istream& stream, std::span<const std::byte> metadata,
                                      std::span<const StreamRange> ranges,
                                      std::stop_token stop_token)
    -> std::expected<std::string, std::string> {
  auto state_result = create_state();
  if (!state_result) {
    return std::unexpected(state_result.error());
  }
  auto state = std::move(state_result.value());

  // 元数据使用固定宽度二进制字段，避免字符串拼接产生边界歧义。
  auto metadata_result = update_state(state.get(), metadata.data(), metadata.size_bytes());
  if (!metadata_result) {
    return std::unexpected(metadata_result.error());
  }

  std::vector<char> buffer(kReadBufferSize);

  for (const auto& range : ranges) {
    // 每个采样区间开始前响应停止，当前同步读取自然完成。
    if (stop_token.stop_requested()) {
      return std::unexpected("Hash calculation cancelled");
    }

    stream.clear();
    stream.seekg(static_cast<std::streamoff>(range.offset), std::ios::beg);
    if (!stream) {
      return std::unexpected("Input stream seek failed");
    }

    std::size_t remaining = range.size;
    while (remaining > 0) {
      if (stop_token.stop_requested()) {
        return std::unexpected("Hash calculation cancelled");
      }

      auto read_size = std::min(remaining, buffer.size());
      stream.read(buffer.data(), static_cast<std::streamsize>(read_size));
      auto bytes_read = stream.gcount();
      if (bytes_read <= 0) {
        return std::unexpected("Input stream sample read failed");
      }

      auto update_result =
          update_state(state.get(), buffer.data(), static_cast<std::size_t>(bytes_read));
      if (!update_result) {
        return std::unexpected(update_result.error());
      }

      remaining -= static_cast<std::size_t>(bytes_read);
      if (static_cast<std::size_t>(bytes_read) != read_size) {
        return std::unexpected("Input stream sample is shorter than expected");
      }
    }
  }

  return digest_state(state.get());
}

}  // namespace Vendor::XXHash
