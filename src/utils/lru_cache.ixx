module;

export module Utils.LRUCache;

import std;

export namespace Utils::LRUCache {

// 缓存节点
template <typename Key, typename Value>
struct CacheNode {
  Key key;
  Value value;
};

// LRU 缓存状态
template <typename Key, typename Value>
struct LRUCacheState {
  size_t capacity;
  std::unordered_map<Key, typename std::list<CacheNode<Key, Value>>::iterator> map;
  std::list<CacheNode<Key, Value>> list;  // 头部=最新，尾部=最旧
  std::shared_mutex mutex;                // 读写锁
};

// 创建缓存
template <typename Key, typename Value>
auto create(size_t capacity) -> LRUCacheState<Key, Value> {
  return LRUCacheState<Key, Value>{.capacity = capacity, .map = {}, .list = {}};
}

// 查询缓存
template <typename Key, typename Value>
auto get(LRUCacheState<Key, Value>& cache, const Key& key) -> std::optional<Value> {
  std::unique_lock lock(cache.mutex);

  auto it = cache.map.find(key);
  if (it == cache.map.end()) {
    return std::nullopt;
  }

  auto node_it = it->second;
  auto value = node_it->value;  // 拷贝值

  // 移动到链表头部（LRU更新）
  cache.list.splice(cache.list.begin(), cache.list, node_it);

  return value;
}

// 插入缓存（写操作）
template <typename Key, typename Value>
auto put(LRUCacheState<Key, Value>& cache, const Key& key, Value value) -> void {
  std::unique_lock lock(cache.mutex);

  // 如果已存在，更新
  auto it = cache.map.find(key);
  if (it != cache.map.end()) {
    auto node_it = it->second;
    node_it->value = std::move(value);
    cache.list.splice(cache.list.begin(), cache.list, node_it);
    return;
  }

  // 检查容量
  if (cache.list.size() >= cache.capacity) {
    // 淘汰最旧的
    auto& oldest = cache.list.back();
    cache.map.erase(oldest.key);
    cache.list.pop_back();
  }

  // 插入新节点
  cache.list.push_front(CacheNode<Key, Value>{.key = key, .value = std::move(value)});

  cache.map[key] = cache.list.begin();
}

// 批量预热（高效插入多个）
template <typename Key, typename Value>
auto warm_up(LRUCacheState<Key, Value>& cache, const std::vector<std::pair<Key, Value>>& items)
    -> void {
  std::unique_lock lock(cache.mutex);

  for (const auto& [key, value] : items) {
    // 如果已存在，跳过
    if (cache.map.contains(key)) {
      continue;
    }

    // 检查容量
    if (cache.list.size() >= cache.capacity) {
      auto& oldest = cache.list.back();
      cache.map.erase(oldest.key);
      cache.list.pop_back();
    }

    cache.list.push_front(CacheNode<Key, Value>{.key = key, .value = value});

    cache.map[key] = cache.list.begin();
  }
}

// 清空缓存
template <typename Key, typename Value>
auto clear(LRUCacheState<Key, Value>& cache) -> void {
  std::unique_lock lock(cache.mutex);
  cache.map.clear();
  cache.list.clear();
}

// 获取统计信息
template <typename Key, typename Value>
auto get_stats(const LRUCacheState<Key, Value>& cache)
    -> std::tuple<size_t, size_t> {  // (current_size, capacity)
  std::shared_lock lock(cache.mutex);
  return {cache.list.size(), cache.capacity};
}

}  // namespace Utils::LRUCache
