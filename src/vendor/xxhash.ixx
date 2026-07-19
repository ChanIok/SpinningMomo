module;

#include <xxhash.h>

export module Vendor.XXHash;

import std;

namespace Vendor::XXHash {

constexpr std::size_t kReadBufferSize = 1024 * 1024;

// 分块读取输入流并计算 XXH3 哈希，在块边界响应停止且不按输入总大小占用内存
export auto hash_stream_to_hex(std::istream& stream, std::stop_token stop_token)
    -> std::expected<std::string, std::string> {
  // 每次只保留 1 MiB 输入，限制单个哈希任务的内存上限
  std::vector<char> buffer(kReadBufferSize);

  // 将 XXH3 状态绑定到官方释放函数，确保所有提前返回都能清理资源
  auto state =
      std::unique_ptr<XXH3_state_t, decltype(&XXH3_freeState)>(XXH3_createState(), &XXH3_freeState);
  if (!state) {
    return std::unexpected("Failed to create XXH3 state");
  }

  // 初始化一次流式会话，后续所有数据块按顺序写入同一个状态
  if (XXH3_64bits_reset(state.get()) != XXH_OK) {
    return std::unexpected("Failed to reset XXH3 state");
  }

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
      if (XXH3_64bits_update(state.get(), buffer.data(), static_cast<std::size_t>(bytes_read)) !=
          XXH_OK) {
        return std::unexpected("Failed to update XXH3 state");
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
  auto hash = XXH3_64bits_digest(state.get());
  return std::format("{:016x}", hash);
}

}  // namespace Vendor::XXHash
