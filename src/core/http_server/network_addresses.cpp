#include "core/http_server/network_addresses.hpp"

#include "vendor/std.hpp"

#include "vendor/windows/iphlpapi.hpp"
#include "vendor/windows/ws2tcpip.hpp"

#include "utils/string/string.hpp"

namespace core::http_server {

namespace {

// 优先使用稳定的适配器名称，缺失时退回当前生命周期内稳定的 LUID。
auto get_adapter_id(const IP_ADAPTER_ADDRESSES* adapter) -> std::string {
  if (adapter->AdapterName && adapter->AdapterName[0] != '\0') {
    // NetworkGuid 标识当前连接的网络，Wi-Fi 切换网络后可能变化；AdapterName 才是适配器
    // 自身的稳定标识，适合保存为用户的首选项。
    return std::string(adapter->AdapterName);
  }

  // 极少数驱动可能不提供 AdapterName，LUID 仍可在当前适配器生命周期内稳定区分接口。
  return std::format("luid:{:016x}", static_cast<unsigned long long>(adapter->Luid.Value));
}

// 判断地址是否属于 RFC1918 规定的局域网私有地址段。
auto is_private_ipv4(std::string_view ip) -> bool {
  IN_ADDR address{};
  // 无法解析的地址不参与局域网优先级判断。
  if (InetPtonA(AF_INET, ip.data(), &address) != 1) {
    return false;
  }

  // 转为主机字节序后，仅匹配 RFC1918 的三组私有地址段。
  const auto value = ntohl(address.S_un.S_addr);
  const auto first = (value >> 24) & 0xff;
  const auto second = (value >> 16) & 0xff;

  return first == 10 || (first == 172 && second >= 16 && second <= 31) ||
         (first == 192 && second == 168);
}

// 只依据 Windows 明确的接口类型标记虚拟网卡，不根据名称猜测具体软件。
auto is_virtual_adapter(const IP_ADAPTER_ADDRESSES* adapter) -> bool {
  // 这些接口类型由系统明确定义，不依赖 FriendlyName 是否包含某个产品名。
  return adapter->IfType == IF_TYPE_TUNNEL || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
         adapter->IfType == IF_TYPE_PPP;
}

// 检查适配器是否声明了 IPv4 默认网关。
auto has_ipv4_gateway(const IP_ADAPTER_ADDRESSES* adapter) -> bool {
  for (auto* gateway = adapter->FirstGatewayAddress; gateway; gateway = gateway->Next) {
    if (gateway->Address.lpSockaddr && gateway->Address.lpSockaddr->sa_family == AF_INET) {
      return true;
    }
  }
  return false;
}

// 将单个单播地址转换为 IPv4 文本，并过滤回环和 APIPA 地址。
auto get_ipv4(const IP_ADAPTER_UNICAST_ADDRESS* unicast) -> std::optional<std::string> {
  if (!unicast || !unicast->Address.lpSockaddr ||
      unicast->Address.lpSockaddr->sa_family != AF_INET) {
    return std::nullopt;
  }

  const auto* address = reinterpret_cast<const sockaddr_in*>(unicast->Address.lpSockaddr);
  char buffer[INET_ADDRSTRLEN]{};
  if (!InetNtopA(AF_INET, const_cast<IN_ADDR*>(&address->sin_addr), buffer, sizeof(buffer))) {
    return std::nullopt;
  }

  const auto value = std::string(buffer);
  if (value.starts_with("127.") || value.starts_with("169.254.")) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

// 枚举当前可用 IPv4 地址，并按首选项、RFC1918 地址、网关和 metric 排序。
auto enumerate_network_addresses(std::string_view preferred_adapter_id)
    -> std::expected<std::vector<NetworkAddress>, std::string> {
  // 同时请求网关信息，后续排序才能区分真正具备外连路径的接口。
  constexpr ULONG flags =
      GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG buffer_size = 16 * 1024;
  std::vector<std::byte> buffer(buffer_size);

  // Windows 可能在第一次调用时返回更大的需求缓冲区，因此循环扩容重试。
  ULONG status = ERROR_BUFFER_OVERFLOW;
  while (status == ERROR_BUFFER_OVERFLOW) {
    status =
        GetAdaptersAddresses(AF_INET, flags, nullptr,
                             reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &buffer_size);
    if (status == ERROR_BUFFER_OVERFLOW) {
      buffer.resize(buffer_size);
    }
  }

  if (status != NO_ERROR) {
    return std::unexpected(std::format("GetAdaptersAddresses failed: {}", status));
  }

  // 只保留已启用的物理或逻辑接口，逐个检查其 IPv4 单播地址。
  std::vector<NetworkAddress> addresses;
  for (auto* adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()); adapter;
       adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
      continue;
    }

    const auto adapter_id = get_adapter_id(adapter);
    const auto adapter_name = adapter->FriendlyName
                                  ? utils::string::ToUtf8(std::wstring(adapter->FriendlyName))
                                  : std::string{};
    const auto gateway = has_ipv4_gateway(adapter);
    const auto is_virtual = is_virtual_adapter(adapter);

    // 一个适配器可能同时拥有多个地址，每个地址都单独参与排序。
    for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
      auto ip = get_ipv4(unicast);
      if (!ip) {
        continue;
      }

      auto ip_value = std::move(*ip);
      addresses.push_back(NetworkAddress{
          .adapter_id = adapter_id,
          .adapter_name = adapter_name,
          .ip = std::move(ip_value),
          .metric = adapter->Ipv4Metric,
          .has_default_gateway = gateway,
          // 地址段本身不能证明接口是虚拟网卡；例如企业网常用 172.16/12。
          .is_virtual = is_virtual,
          .is_private = false,
          .is_preferred = adapter_id == preferred_adapter_id,
      });
      addresses.back().is_private = is_private_ipv4(addresses.back().ip);
    }
  }

  // 自动选择只依赖可验证的网络属性，避免把 Clash、WSL 等产品名称当成规则。
  std::ranges::sort(addresses, [](const auto& lhs, const auto& rhs) {
    if (lhs.is_preferred != rhs.is_preferred) return lhs.is_preferred > rhs.is_preferred;
    // RFC1918 地址更可能是同一局域网内其他设备可访问的地址。
    if (lhs.is_private != rhs.is_private) return lhs.is_private > rhs.is_private;
    // 同一地址类别下，优先选择有默认路由的接口。
    if (lhs.has_default_gateway != rhs.has_default_gateway) {
      return lhs.has_default_gateway > rhs.has_default_gateway;
    }
    // 仍无法区分时，使用 Windows 提供的接口 metric。
    if (lhs.metric != rhs.metric) return lhs.metric < rhs.metric;
    // 最后用稳定适配器标识和地址保证结果可重复。
    if (lhs.adapter_id != rhs.adapter_id) return lhs.adapter_id < rhs.adapter_id;
    return lhs.ip < rhs.ip;
  });

  return addresses;
}

}  // namespace core::http_server
