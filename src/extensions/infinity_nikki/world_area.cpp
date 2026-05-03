module;

module Extensions.InfinityNikki.WorldArea;

import std;
import Core.HttpClient;
import Core.HttpClient.Types;
import Core.State;
import Extensions.InfinityNikki.Types;
import Utils.Logger;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Extensions::InfinityNikki::WorldArea {

// 远端地图配置的来源地址和内存缓存有效期。
// 配置包含所有世界的 polygon 区域、坐标变换参数和官方 world_id 映射，
// 替代原先硬编码在源码中的规则。
constexpr std::string_view kMapConfigUrl = "https://api.infinitymomo.com/api/v1/map.json";
constexpr auto kMapConfigTtl = std::chrono::hours(6);

// 进程级单例缓存。首次加载失败时返回错误；后续刷新失败时回退到上一次成功缓存。
struct MapConfigCache {
  std::mutex mutex;
  std::optional<InfinityNikkiMapConfig> cached_config;
  std::chrono::steady_clock::time_point last_updated = std::chrono::steady_clock::time_point::min();
};

auto map_config_cache() -> MapConfigCache& {
  static MapConfigCache cache;
  return cache;
}

auto is_cached_config_fresh(const MapConfigCache& cache) -> bool {
  if (!cache.cached_config.has_value()) {
    return false;
  }
  return (std::chrono::steady_clock::now() - cache.last_updated) < kMapConfigTtl;
}

auto trim_ascii_copy(std::string_view value) -> std::string {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

  std::size_t start = 0;
  while (start < value.size() && is_space(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && is_space(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

// 将 world_id 标准化为纯数字主版本（去掉小数点及之后的部分，如 "10000001.1" → "10000001"）。
// 数据库和远端配置中的 world_id 可能带有版本号后缀，统一去掉以进行匹配。
auto normalize_world_id(std::string_view world_id) -> std::string {
  auto normalized_world_id = trim_ascii_copy(world_id);
  if (const auto dot_pos = normalized_world_id.find('.'); dot_pos != std::string::npos) {
    normalized_world_id = normalized_world_id.substr(0, dot_pos);
  }
  return normalized_world_id;
}

// 标准化官方 world_id（保留版本号，只去除首尾空白和包裹双引号）。
// localStorage 等处可能存储带引号的值如 "\"1010202.1\""。
auto normalize_official_world_id(std::string_view world_id) -> std::string {
  auto normalized_world_id = trim_ascii_copy(world_id);
  if (normalized_world_id.size() >= 2 && normalized_world_id.front() == '"' &&
      normalized_world_id.back() == '"') {
    normalized_world_id = trim_ascii_copy(
        std::string_view(normalized_world_id).substr(1, normalized_world_id.size() - 2));
  }
  return normalized_world_id;
}

// 校验官方 world_id 格式：纯数字或 "数字.数字"（如 "1.1"、"10000001.1"）。
auto is_valid_official_world_id(std::string_view world_id) -> bool {
  if (world_id.empty()) {
    return false;
  }

  bool saw_dot = false;
  std::size_t digits_before_dot = 0;
  std::size_t digits_after_dot = 0;

  for (const auto ch : world_id) {
    if (ch == '.') {
      if (saw_dot) {
        return false;
      }
      saw_dot = true;
      continue;
    }

    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }

    if (saw_dot) {
      ++digits_after_dot;
    } else {
      ++digits_before_dot;
    }
  }

  return digits_before_dot > 0 && (!saw_dot || digits_after_dot > 0);
}

// 校验单条区域规则：polygon 至少 3 个点、坐标有限、z_range 合法。
auto validate_rule(const InfinityNikkiMapWorldRule& rule, std::string_view world_id,
                   std::size_t rule_index) -> std::expected<void, std::string> {
  if (rule.polygon.size() < 3) {
    return std::unexpected(
        std::format("Map config world '{}' rule {} has fewer than 3 points", world_id, rule_index));
  }

  for (const auto& point : rule.polygon) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      return std::unexpected(
          std::format("Map config world '{}' rule {} has invalid point", world_id, rule_index));
    }
  }

  if (rule.z_range.has_value()) {
    const auto& z_range = *rule.z_range;
    if (z_range.min.has_value() && !std::isfinite(*z_range.min)) {
      return std::unexpected(std::format("Map config world '{}' rule {} has invalid zRange.min",
                                         world_id, rule_index));
    }
    if (z_range.max.has_value() && !std::isfinite(*z_range.max)) {
      return std::unexpected(std::format("Map config world '{}' rule {} has invalid zRange.max",
                                         world_id, rule_index));
    }
    if (z_range.min.has_value() && z_range.max.has_value() && *z_range.min > *z_range.max) {
      return std::unexpected(
          std::format("Map config world '{}' rule {} has zRange.min greater than zRange.max",
                      world_id, rule_index));
    }
  }

  return {};
}

// 校验并标准化整个地图配置：检查 schema 版本、world_id 唯一性、
// default_world_id 有效性、坐标参数合法性，以及每条规则的 polygon/z_range。
auto validate_and_normalize_config(InfinityNikkiMapConfig config)
    -> std::expected<InfinityNikkiMapConfig, std::string> {
  if (config.schema_version != 1) {
    return std::unexpected("Unsupported Infinity Nikki map config schema version: " +
                           std::to_string(config.schema_version));
  }

  config.default_world_id = normalize_world_id(config.default_world_id);
  if (config.default_world_id.empty()) {
    return std::unexpected("Infinity Nikki map config default_world_id is empty");
  }

  if (config.worlds.empty()) {
    return std::unexpected("Infinity Nikki map config contains no worlds");
  }

  std::unordered_set<std::string> world_ids;
  bool found_default_world = false;

  for (auto& world : config.worlds) {
    world.world_id = normalize_world_id(world.world_id);
    world.official_world_id = normalize_official_world_id(world.official_world_id);

    if (world.world_id.empty()) {
      return std::unexpected("Infinity Nikki map config contains a world with empty world_id");
    }
    if (!is_valid_official_world_id(world.official_world_id)) {
      return std::unexpected("Infinity Nikki map config world '" + world.world_id +
                             "' has invalid official_world_id");
    }
    if (!world_ids.insert(world.world_id).second) {
      return std::unexpected("Infinity Nikki map config has duplicated world_id: " +
                             world.world_id);
    }
    const auto& coordinate = world.coordinate;
    if (!std::isfinite(coordinate.x_scale) || coordinate.x_scale == 0.0 ||
        !std::isfinite(coordinate.x_bias) || !std::isfinite(coordinate.y_scale) ||
        coordinate.y_scale == 0.0 || !std::isfinite(coordinate.y_bias)) {
      return std::unexpected("Infinity Nikki map config world '" + world.world_id +
                             "' has invalid coordinate profile");
    }

    if (world.world_id == config.default_world_id) {
      found_default_world = true;
    }

    for (std::size_t index = 0; index < world.rules.size(); ++index) {
      if (auto rule_result = validate_rule(world.rules[index], world.world_id, index);
          !rule_result) {
        return std::unexpected(rule_result.error());
      }
    }
  }

  if (!found_default_world) {
    return std::unexpected("Infinity Nikki map config default_world_id is not present in worlds");
  }

  return config;
}

auto parse_map_config_payload(const std::string& body)
    -> std::expected<InfinityNikkiMapConfig, std::string> {
  auto parsed =
      rfl::json::read<InfinityNikkiMapConfig, rfl::SnakeCaseToCamelCase, rfl::DefaultIfMissing>(
          body);
  if (!parsed) {
    return std::unexpected("Failed to parse Infinity Nikki map config: " + parsed.error().what());
  }

  return validate_and_normalize_config(std::move(parsed.value()));
}

// 从远端拉取地图配置 JSON 并解析、校验。
auto fetch_and_parse_map_config(Core::State::AppState& app_state)
    -> asio::awaitable<std::expected<InfinityNikkiMapConfig, std::string>> {
  Core::HttpClient::Types::Request request{
      .method = "GET",
      .url = std::string(kMapConfigUrl),
      .headers = {Core::HttpClient::Types::Header{.name = "Accept", .value = "application/json"}},
  };

  auto response = co_await Core::HttpClient::fetch(app_state, request);
  if (!response) {
    co_return std::unexpected("Failed to fetch Infinity Nikki map config: " + response.error());
  }

  if (response->status_code < 200 || response->status_code >= 300) {
    co_return std::unexpected("Infinity Nikki map config returned non-2xx response: " +
                              std::to_string(response->status_code));
  }

  co_return parse_map_config_payload(response->body);
}

// 带 TTL 缓存的地图配置加载入口。
// 缓存命中直接返回；未命中则拉取远端配置；
// 拉取失败时若有旧缓存则回退使用（降级），否则返回错误。
auto load_map_config(Core::State::AppState& app_state)
    -> asio::awaitable<std::expected<InfinityNikkiMapConfig, std::string>> {
  auto& cache = map_config_cache();

  {
    std::lock_guard<std::mutex> lock(cache.mutex);
    if (is_cached_config_fresh(cache)) {
      co_return *cache.cached_config;
    }
  }

  auto fetched = co_await fetch_and_parse_map_config(app_state);
  if (!fetched) {
    std::lock_guard<std::mutex> lock(cache.mutex);
    if (cache.cached_config.has_value()) {
      Logger().warn("Use stale Infinity Nikki map config because refresh failed: {}",
                    fetched.error());
      co_return *cache.cached_config;
    }
    co_return std::unexpected(fetched.error());
  }

  {
    std::lock_guard<std::mutex> lock(cache.mutex);
    cache.cached_config = fetched.value();
    cache.last_updated = std::chrono::steady_clock::now();
    co_return *cache.cached_config;
  }
}

// 根据 world_id 查找世界配置，返回指针（未找到返回 nullptr）。
auto find_world(const InfinityNikkiMapConfig& config, std::string_view world_id)
    -> const InfinityNikkiMapWorld* {
  const auto normalized_world_id = normalize_world_id(world_id);
  if (normalized_world_id.empty()) {
    return nullptr;
  }

  for (const auto& world : config.worlds) {
    if (world.world_id == normalized_world_id) {
      return &world;
    }
  }

  return nullptr;
}

// 射线法（ray casting）判断点是否在多边形内部。
auto is_point_in_polygon(double x, double y,
                         const std::vector<InfinityNikkiMapPolygonPoint>& polygon) -> bool {
  if (polygon.size() < 3) {
    return false;
  }

  bool is_inside = false;
  for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const auto& current_point = polygon[i];
    const auto& previous_point = polygon[j];

    const auto yi = current_point.y;
    const auto xi = current_point.x;
    const auto yj = previous_point.y;
    const auto xj = previous_point.x;

    const bool intersects = (yi > y) != (yj > y) &&
                            x < ((xj - xi) * (y - yi)) / (yj - yi == 0.0 ? 1e-12 : (yj - yi)) + xi;
    if (intersects) {
      is_inside = !is_inside;
    }
  }

  return is_inside;
}

// 根据游戏坐标推断所属世界：依次遍历每个世界的规则，
// 先检查 z_range 再检查 polygon，首条命中即返回；
// 全部未命中则回退到 config.default_world_id。
auto resolve_world_or_default(const InfinityNikkiMapConfig& config, const GamePoint& point)
    -> const InfinityNikkiMapWorld* {
  for (const auto& world : config.worlds) {
    for (const auto& rule : world.rules) {
      if (rule.z_range.has_value()) {
        const auto& z_range = *rule.z_range;
        const bool has_min = z_range.min.has_value() && std::isfinite(*z_range.min);
        const bool has_max = z_range.max.has_value() && std::isfinite(*z_range.max);
        if ((has_min || has_max) && (!point.z.has_value() || !std::isfinite(*point.z))) {
          continue;
        }
        if (has_min && *point.z < *z_range.min) {
          continue;
        }
        if (has_max && *point.z > *z_range.max) {
          continue;
        }
      }

      if (is_point_in_polygon(point.x, point.y, rule.polygon)) {
        return &world;
      }
    }
  }

  return find_world(config, config.default_world_id);
}

// 将游戏坐标通过线性变换转为地图经纬度。
// 官方地图的经纬度约定与游戏坐标轴向相反，所以 map_x → lng, map_y → lat。
auto transform_game_to_map_coordinates(const GamePoint& point, const InfinityNikkiMapWorld& world)
    -> std::expected<MapCoordinate, std::string> {
  const auto& profile = world.coordinate;
  const auto map_x = point.x * profile.x_scale + profile.x_bias;
  const auto map_y = point.y * profile.y_scale + profile.y_bias;
  if (!std::isfinite(map_x) || !std::isfinite(map_y)) {
    return std::unexpected("Map coordinate transform produced non-finite result");
  }

  return MapCoordinate{
      .lat = map_y,
      .lng = map_x,
  };
}

}  // namespace Extensions::InfinityNikki::WorldArea
