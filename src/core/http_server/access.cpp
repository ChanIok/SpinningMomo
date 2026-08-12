#include "core/http_server/access.hpp"

#include "vendor/std.hpp"

#include "vendor/windows/bcrypt.hpp"

#include "core/http_server/network_addresses.hpp"
#include "core/http_server/sse_manager.hpp"
#include "core/http_server/state.hpp"
#include "core/state/app_state.hpp"
#include "features/settings/state.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"

namespace core::http_server::access {

namespace {

constexpr std::string_view kTokenFileName = "lan-access-token.txt";
constexpr std::string_view kCookieName = "spinning_momo_lan_token";

constexpr std::string_view kBase58Alphabet =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// 使用 Windows 系统随机源生成 8 位 Base58 访问令牌。
auto generate_token() -> std::expected<std::string, std::string> {
  std::array<unsigned char, 8> bytes{};
  const auto status = BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (status < 0) {
    return std::unexpected(std::format("BCryptGenRandom failed, NTSTATUS=0x{:08X}",
                                       static_cast<unsigned long>(status)));
  }

  std::string token;
  token.reserve(bytes.size());
  for (const auto byte : bytes) {
    token += kBase58Alphabet[byte % kBase58Alphabet.size()];
  }
  return token;
}

// 解析应用数据目录中的令牌文件路径。
auto get_token_path() -> std::expected<std::filesystem::path, std::string> {
  return utils::path::GetAppDataFilePath(kTokenFileName);
}

// 用截断写入方式保存当前令牌，避免旧令牌残留在文件尾部。
auto save_token(std::string_view token) -> std::expected<void, std::string> {
  auto path_result = get_token_path();
  if (!path_result) {
    return std::unexpected(path_result.error());
  }

  std::ofstream file(path_result.value(), std::ios::binary | std::ios::trunc);
  if (!file) {
    return std::unexpected("Failed to open LAN access token file for writing");
  }
  file << token << '\n';
  if (!file) {
    return std::unexpected("Failed to write LAN access token file");
  }
  return {};
}

// 读取持久化令牌；文件缺失或格式不符（如 64 位旧令牌）时生成并保存新令牌。
auto load_token() -> std::expected<std::string, std::string> {
  auto path_result = get_token_path();
  if (!path_result) {
    return std::unexpected(path_result.error());
  }

  std::ifstream file(path_result.value(), std::ios::binary);
  if (file) {
    std::string token;
    std::getline(file, token);
    if (token.size() == 8) {
      return token;
    }
    Logger().warn("Ignoring legacy or invalid LAN access token file; generating a new token");
  }

  auto generated = generate_token();
  if (!generated) {
    return std::unexpected(generated.error());
  }
  if (auto save_result = save_token(*generated); !save_result) {
    return std::unexpected(save_result.error());
  }
  return *generated;
}

// 比较两个等长字符串时始终遍历完整内容。
auto constant_time_equal(std::string_view lhs, std::string_view rhs) -> bool {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  unsigned char difference = 0;
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    difference |= static_cast<unsigned char>(lhs[index] ^ rhs[index]);
  }
  return difference == 0;
}

// 去除 Cookie 等 ASCII 协议字段两侧的空白。
auto trim_ascii(std::string_view value) -> std::string_view {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }
  return value;
}

}  // namespace

// 启动 HTTP 服务前准备本次运行使用的 LAN 访问令牌。
auto initialize(core::AppState& state) -> std::expected<void, std::string> {
  if (!state.http_server) {
    return std::unexpected("HTTP server state is not allocated");
  }

  // 先读取持久化令牌，只有成功后才更新运行时状态。
  auto token_result = load_token();
  if (!token_result) {
    return std::unexpected(token_result.error());
  }

  // 在共享状态上加锁，保证 HTTP 请求读取令牌时不会看到半成品。
  {
    std::unique_lock lock(state.http_server->access_token_mutex);
    state.http_server->access_token = std::move(*token_result);
  }

  return {};
}

// 轮换 LAN 令牌并通过 HTTP 事件循环关闭旧的 SSE 会话。
auto reset_token(core::AppState& state) -> std::expected<std::string, std::string> {
  if (!state.http_server) {
    return std::unexpected("HTTP server state is not allocated");
  }

  // 先生成并持久化新令牌，持久化失败时保留旧会话。
  auto generated = generate_token();
  if (!generated) {
    return std::unexpected(generated.error());
  }
  if (auto save_result = save_token(*generated); !save_result) {
    return std::unexpected(save_result.error());
  }

  // 发布新令牌后，旧 Cookie 会立即失效。
  {
    std::unique_lock lock(state.http_server->access_token_mutex);
    state.http_server->access_token = *generated;
  }
  // 旧 Cookie 仍可能保持着 SSE 长连接；令牌轮换后立即撤销已有 HTTP 会话。
  core::http_server::sse_manager::request_close_all_connections(state);
  Logger().info("LAN access token was regenerated");
  return *generated;
}

// 读取设置页所需的当前服务状态、令牌和实时网卡地址。
auto get_runtime_info(const core::AppState& state) -> std::expected<RuntimeInfo, std::string> {
  if (!state.http_server) {
    return std::unexpected("HTTP server state is not allocated");
  }

  RuntimeInfo result;
  std::string preferred_adapter_id;
  // 在同一把设置锁下读取开关和首选网卡，避免两项配置来自不同版本。
  if (state.settings) {
    std::scoped_lock lock(state.settings->mutation_mutex);
    result.configured_enabled = state.settings->raw.app.lan_access.enabled;
    preferred_adapter_id = state.settings->raw.app.lan_access.preferred_adapter_id;
  }
  result.runtime_enabled =
      state.http_server->is_running && state.http_server->runtime_lan_enabled.load();
  result.port = state.http_server->port;
  // 地址不缓存，确保用户切换网卡后刷新页面即可看到最新结果。
  auto addresses_result = enumerate_network_addresses(preferred_adapter_id);
  if (!addresses_result) {
    Logger().warn("Failed to enumerate LAN addresses: {}", addresses_result.error());
  } else {
    result.addresses = std::move(*addresses_result);
  }

  // 令牌单独受锁保护，避免与轮换操作并发读取。
  {
    std::shared_lock lock(state.http_server->access_token_mutex);
    result.token = state.http_server->access_token;
  }

  // 配置值与当前监听范围不一致时保留提示，供重绑失败等异常情况使用。
  result.restart_required = result.configured_enabled != result.runtime_enabled;
  return result;
}

// 识别 IPv4、IPv6 和 IPv4-mapped IPv6 的回环地址。
auto is_loopback_address(std::string_view address) -> bool {
  address = trim_ascii(address);
  if (address == "::1" || address == "0:0:0:0:0:0:0:1") {
    return true;
  }
  if (address.starts_with("::ffff:")) {
    address.remove_prefix(7);
  }
  return address == "127.0.0.1" || address.starts_with("127.");
}

// 判断服务是否已实际监听局域网且配置仍处于启用状态。
auto is_remote_access_enabled(const core::AppState& state) -> bool {
  if (!state.http_server || !state.http_server->is_running ||
      !state.http_server->runtime_lan_enabled.load()) {
    return false;
  }

  if (!state.settings) {
    return false;
  }

  std::scoped_lock lock(state.settings->mutation_mutex);
  return state.settings->raw.app.lan_access.enabled;
}

// 按分号拆分 Cookie 头并返回指定 Cookie 的去空白值。
auto extract_cookie(std::string_view cookie_header, std::string_view name)
    -> std::optional<std::string> {
  while (!cookie_header.empty()) {
    const auto separator = cookie_header.find(';');
    const auto part = trim_ascii(cookie_header.substr(0, separator));
    const auto equals = part.find('=');
    if (equals != std::string_view::npos && trim_ascii(part.substr(0, equals)) == name) {
      return std::string(trim_ascii(part.substr(equals + 1)));
    }
    if (separator == std::string_view::npos) {
      break;
    }
    cookie_header.remove_prefix(separator + 1);
  }
  return std::nullopt;
}

// 读取当前令牌并以常数时间比较请求中的令牌。
auto is_token_valid(const core::AppState& state, std::string_view token) -> bool {
  if (!state.http_server || token.empty()) {
    return false;
  }

  std::shared_lock lock(state.http_server->access_token_mutex);
  return constant_time_equal(token, state.http_server->access_token);
}

// 先信任回环请求，再对远端请求同时检查服务开关和会话令牌。
auto resolve_http_access(const core::AppState& state, std::string_view remote_address,
                         std::string_view cookie_header) -> std::optional<core::rpc::AccessLevel> {
  // WebView2 和本机浏览器不需要额外令牌即可访问本机能力。
  if (is_loopback_address(remote_address)) {
    return core::rpc::AccessLevel::local;
  }

  // 关闭设置后立即撤销远程会话，即使监听范围尚未因重启恢复到 loopback。
  if (!is_remote_access_enabled(state)) {
    return std::nullopt;
  }

  // 远端必须携带经 /t/<token> 交换得到的 HttpOnly Cookie。
  const auto token = extract_cookie(cookie_header, kCookieName);
  if (!token || !is_token_valid(state, *token)) {
    return std::nullopt;
  }
  return core::rpc::AccessLevel::lan;
}

// 生成带 HttpOnly、严格 SameSite 和有限生命周期的会话 Cookie。
auto make_cookie(std::string_view token) -> std::string {
  return std::format("{}={}; HttpOnly; SameSite=Strict; Path=/; Max-Age=2592000", kCookieName,
                     token);
}

}  // namespace core::http_server::access
