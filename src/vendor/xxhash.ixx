module;

#include <xxhash.h>

export module Vendor.XXHash;

import std;

namespace Vendor::XXHash {

// 计算字符向量的哈希值并返回16进制字符串 - scanner.cpp中实际使用的函数
export auto HashCharVectorToHex(const std::vector<char>& data) -> std::string {
  auto hash = XXH3_64bits(data.data(), data.size());
  return std::format("{:016x}", hash);
}

}  // namespace Vendor::XXHash