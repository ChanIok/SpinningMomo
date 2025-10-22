module;

export module Features.Gallery.State;

import std;
import Utils.LRUCache;

export namespace Features::Gallery::State {

struct GalleryState {
  // 缩略图目录路径
  std::filesystem::path thumbnails_directory;

  // 原图路径缓存 (asset_id -> filesystem::path)
  Utils::LRUCache::LRUCacheState<std::int64_t, std::filesystem::path> image_path_cache{
      .capacity = 5000, .map = {}, .list = {}};
};

}  // namespace Features::Gallery::State
