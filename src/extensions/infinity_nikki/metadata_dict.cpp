module;

module Extensions.InfinityNikki.MetadataDict;

import std;
import Core.HttpClient;
import Core.HttpClient.Types;
import Core.State;
import Extensions.InfinityNikki.Types;
import Utils.Logger;
import <asio.hpp>;
import <rfl/json.hpp>;

namespace Extensions::InfinityNikki::MetadataDict {

// 远端在线字典地址：不随客户端打包发布。
constexpr std::string_view kDictionaryUrl =
    "https://api.infinitymomo.com/api/v1/camera-spinning-momo.json";
// 字典在内存中的有效期，过期后会尝试后台刷新。
constexpr auto kDictionaryTtl = std::chrono::hours(6);

struct MetadataItem {
  std::optional<std::string> zh;
  std::optional<std::string> en;
};

struct MetadataPayload {
  // 与远端 JSON 顶层键保持一致（poses / filters / lights）
  std::optional<std::unordered_map<std::string, MetadataItem>> poses;
  std::optional<std::unordered_map<std::string, MetadataItem>> filters;
  std::optional<std::unordered_map<std::string, MetadataItem>> lights;
};

struct MetadataDictionary {
  std::unordered_map<std::string, MetadataItem> poses;
  std::unordered_map<std::string, MetadataItem> filters;
  std::unordered_map<std::string, MetadataItem> lights;
};

struct MetadataDictionaryCache {
  std::mutex mutex;
  std::optional<MetadataDictionary> cached_dictionary;
  std::chrono::steady_clock::time_point last_updated = std::chrono::steady_clock::time_point::min();
};

auto dictionary_cache() -> MetadataDictionaryCache& {
  // 进程级单例缓存：整个应用共享一份在线字典。
  static MetadataDictionaryCache cache;
  return cache;
}

auto is_cached_dictionary_fresh(const MetadataDictionaryCache& cache) -> bool {
  if (!cache.cached_dictionary.has_value()) {
    return false;
  }
  return (std::chrono::steady_clock::now() - cache.last_updated) < kDictionaryTtl;
}

auto resolve_name(const std::unordered_map<std::string, MetadataItem>& dictionary,
                  const std::optional<std::int64_t>& id, std::string_view locale_key)
    -> std::optional<std::string> {
  if (!id.has_value()) {
    return std::nullopt;
  }

  auto it = dictionary.find(std::to_string(*id));
  if (it == dictionary.end()) {
    return std::nullopt;
  }

  const auto& item = it->second;
  // 优先返回当前语言；若缺失则回退到另一语言，尽量避免空展示。
  if (locale_key == "zh") {
    if (item.zh.has_value() && !item.zh->empty()) return item.zh;
    if (item.en.has_value() && !item.en->empty()) return item.en;
    return std::nullopt;
  }

  if (item.en.has_value() && !item.en->empty()) return item.en;
  if (item.zh.has_value() && !item.zh->empty()) return item.zh;
  return std::nullopt;
}

auto to_locale_key(std::optional<std::string> locale) -> std::string_view {
  // 后端只需要区分中/英文两档，简化映射逻辑。
  if (locale.has_value() && locale->rfind("zh", 0) == 0) {
    return "zh";
  }
  return "en";
}

auto parse_dictionary_payload(const std::string& body)
    -> std::expected<MetadataDictionary, std::string> {
  auto parsed = rfl::json::read<MetadataPayload, rfl::DefaultIfMissing>(body);
  if (!parsed) {
    return std::unexpected("Failed to parse camera metadata dictionary: " + parsed.error().what());
  }

  MetadataDictionary dictionary;
  // 使用空 map 兜底，保证下游 lookup 总是可执行。
  dictionary.poses =
      std::move(parsed->poses.value_or(std::unordered_map<std::string, MetadataItem>{}));
  dictionary.filters =
      std::move(parsed->filters.value_or(std::unordered_map<std::string, MetadataItem>{}));
  dictionary.lights =
      std::move(parsed->lights.value_or(std::unordered_map<std::string, MetadataItem>{}));
  return dictionary;
}

auto fetch_and_parse_dictionary(Core::State::AppState& app_state)
    -> asio::awaitable<std::expected<MetadataDictionary, std::string>> {
  // 只读取公开 JSON，不带业务副作用。
  Core::HttpClient::Types::Request request{
      .method = "GET",
      .url = std::string(kDictionaryUrl),
      .headers = {Core::HttpClient::Types::Header{.name = "Accept", .value = "application/json"}},
  };

  auto response = co_await Core::HttpClient::fetch(app_state, request);
  if (!response) {
    co_return std::unexpected("Failed to fetch camera metadata dictionary: " + response.error());
  }

  if (response->status_code < 200 || response->status_code >= 300) {
    // 非 2xx 统一视为失败，交给上层做缓存回退。
    co_return std::unexpected("Camera metadata dictionary returned non-2xx response: " +
                              std::to_string(response->status_code));
  }

  co_return parse_dictionary_payload(response->body);
}

auto load_dictionary(Core::State::AppState& app_state)
    -> asio::awaitable<std::expected<MetadataDictionary, std::string>> {
  auto& cache = dictionary_cache();

  // 热路径：优先返回新鲜缓存，避免在详情面板频繁切换时反复请求远端字典。
  {
    std::lock_guard<std::mutex> lock(cache.mutex);
    if (is_cached_dictionary_fresh(cache)) {
      co_return *cache.cached_dictionary;
    }
  }

  auto fetched = co_await fetch_and_parse_dictionary(app_state);
  if (!fetched) {
    std::lock_guard<std::mutex> lock(cache.mutex);
    // 网络失败时优先回退旧缓存，保证 UI 至少能继续显示上一次可用映射。
    if (cache.cached_dictionary.has_value()) {
      Logger().warn("Use stale Infinity Nikki metadata dictionary because refresh failed: {}",
                    fetched.error());
      co_return *cache.cached_dictionary;
    }
    co_return std::unexpected(fetched.error());
  }

  {
    std::lock_guard<std::mutex> lock(cache.mutex);
    // 刷新成功后原子替换缓存并更新时间戳。
    cache.cached_dictionary = fetched.value();
    cache.last_updated = std::chrono::steady_clock::now();
    co_return *cache.cached_dictionary;
  }
}

auto resolve_metadata_names(Core::State::AppState& app_state,
                            const GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiMetadataNames, std::string>> {
  // 统一入口：先拿字典，再按传入 id + locale 做“最小响应”映射。
  auto dictionary = co_await load_dictionary(app_state);
  if (!dictionary) {
    co_return std::unexpected(dictionary.error());
  }

  auto locale_key = to_locale_key(params.locale);

  co_return InfinityNikkiMetadataNames{
      .filter_name = resolve_name(dictionary->filters, params.filter_id, locale_key),
      .pose_name = resolve_name(dictionary->poses, params.pose_id, locale_key),
      .light_name = resolve_name(dictionary->lights, params.light_id, locale_key),
  };
}

}  // namespace Extensions::InfinityNikki::MetadataDict
