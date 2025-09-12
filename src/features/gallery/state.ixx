module;

export module Features.Gallery.State;

import std;

export namespace Features::Gallery::State {

// gallery模块状态 - 只包含缩略图路径
struct GalleryState {
  // 缩略图目录路径
  std::filesystem::path thumbnails_directory;
};

}  // namespace Features::Gallery::State
