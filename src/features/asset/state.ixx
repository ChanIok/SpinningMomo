module;

export module Features.Asset.State;

import std;

export namespace Features::Asset::State {

// 资产模块状态 - 只包含缩略图路径
struct AssetState {
  // 缩略图目录路径
  std::filesystem::path thumbnails_directory;
};

}  // namespace Features::Asset::State