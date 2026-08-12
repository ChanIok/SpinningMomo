#pragma once

#include "vendor/std.hpp"

namespace core::http_server {

// 描述一个可用于生成局域网访问链接的 IPv4 地址及其排序依据。
struct NetworkAddress {
  std::string adapter_id;            // 稳定适配器标识，用于保存用户首选项
  std::string adapter_name;          // Windows 适配器友好名称
  std::string ip;                    // 可供其他设备访问的 IPv4 地址
  std::uint32_t metric = 0;          // Windows IPv4 接口 metric
  bool has_default_gateway = false;  // 是否存在 IPv4 默认网关
  bool is_virtual = false;           // 是否由 Windows 接口类型明确标记为虚拟接口
  bool is_private = false;           // 是否属于 RFC1918 局域网私有地址段
  bool is_preferred = false;         // 是否命中用户首选适配器
};

// 枚举可用于局域网访问的 IPv4 地址，并按首选项、RFC1918 地址和路由质量排序。
auto enumerate_network_addresses(std::string_view preferred_adapter_id)
    -> std::expected<std::vector<NetworkAddress>, std::string>;

}  // namespace core::http_server
