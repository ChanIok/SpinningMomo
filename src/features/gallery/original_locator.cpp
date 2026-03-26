module;

module Features.Gallery.OriginalLocator;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Folder.Repository;
import Utils.Logger;
import Utils.Path;

namespace Features::Gallery::OriginalLocator {

namespace Detail {

// 读取图库中的所有 root folders。
// 当前项目里，parent_id 为空的 folder 就代表一个 watch root。
// 这里按路径长度倒序排序，避免较短前缀先匹配到错误的 root。
auto load_root_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string> {
  auto folders_result = Features::Gallery::Folder::Repository::list_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected("Failed to load folders for original locator: " +
                           folders_result.error());
  }

  std::vector<Types::Folder> root_folders;
  for (const auto& folder : folders_result.value()) {
    if (!folder.parent_id.has_value()) {
      root_folders.push_back(folder);
    }
  }

  std::ranges::sort(root_folders, [](const Types::Folder& lhs, const Types::Folder& rhs) {
    return lhs.path.size() > rhs.path.size();
  });

  return root_folders;
}

// 为单个 asset 推导 originals locator：
// - root_id: 资源属于哪个 watch root
// - relative_path: 文件在该 root 下的相对路径
//
// 这里不会改数据库，只是给 RPC 返回前的运行时对象补齐字段。
auto try_assign_locator_from_roots(const std::vector<Types::Folder>& root_folders,
                                   Types::Asset& asset) -> std::expected<void, std::string> {
  asset.root_id.reset();
  asset.relative_path.reset();

  auto normalized_asset_result = Utils::Path::NormalizePath(std::filesystem::path(asset.path));
  if (!normalized_asset_result) {
    Logger().warn("Failed to normalize asset path for original locator '{}': {}", asset.path,
                  normalized_asset_result.error());
    return {};
  }

  auto normalized_asset_path = normalized_asset_result.value();

  for (const auto& root_folder : root_folders) {
    auto normalized_root_result =
        Utils::Path::NormalizePath(std::filesystem::path(root_folder.path));
    if (!normalized_root_result) {
      Logger().warn("Failed to normalize root folder path for original locator '{}': {}",
                    root_folder.path, normalized_root_result.error());
      continue;
    }

    auto normalized_root_path = normalized_root_result.value();
    if (!Utils::Path::IsPathWithinBase(normalized_asset_path, normalized_root_path)) {
      continue;
    }

    auto relative_path = normalized_asset_path.lexically_relative(normalized_root_path);
    auto relative_path_string = relative_path.generic_string();
    if (relative_path_string.empty() || relative_path_string == "." ||
        relative_path_string.starts_with("../")) {
      Logger().warn(
          "Computed invalid original relative path for asset '{}': root='{}', relative='{}'",
          asset.path, root_folder.path, relative_path_string);
      return {};
    }

    asset.root_id = root_folder.id;
    asset.relative_path = std::move(relative_path_string);
    return {};
  }

  Logger().warn("No gallery root folder matched asset path for original locator: {}", asset.path);
  return {};
}

}  // namespace Detail

// 统一约定每个 root 的 WebView host 名称。
// 例如 root_id=3 时，对应的 host 是 r-3.test。
auto make_root_host_name(std::int64_t root_id) -> std::wstring {
  return std::format(L"r-{}.test", root_id);
}

// 为一批资产补齐 originals locator（root_id + relative_path）。
// 批量走这个接口，root folders 只查一次，避免 N 次重复加载。
auto populate_asset_locators(Core::State::AppState& app_state, std::vector<Types::Asset>& assets)
    -> std::expected<void, std::string> {
  auto root_folders_result = Detail::load_root_folders(app_state);
  if (!root_folders_result) {
    return std::unexpected(root_folders_result.error());
  }

  for (auto& asset : assets) {
    auto assign_result = Detail::try_assign_locator_from_roots(root_folders_result.value(), asset);
    if (!assign_result) {
      return std::unexpected(assign_result.error());
    }
  }

  return {};
}

// 根据 root_id + relative_path 还原真实磁盘路径。
// dev 浏览器模式下的 HTTP originals resolver 会通过它把 URL 映射回文件系统。
auto resolve_original_file_path(Core::State::AppState& app_state, std::int64_t root_id,
                                std::string_view relative_path)
    -> std::expected<std::filesystem::path, std::string> {
  if (root_id <= 0) {
    return std::unexpected("Original root id must be greater than 0");
  }

  if (relative_path.empty()) {
    return std::unexpected("Original relative path is empty");
  }

  auto folder_result = Features::Gallery::Folder::Repository::get_folder_by_id(app_state, root_id);
  if (!folder_result) {
    return std::unexpected("Failed to query original root folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return std::unexpected("Original root folder not found");
  }

  const auto& folder = folder_result->value();
  if (folder.parent_id.has_value()) {
    return std::unexpected("Original root id does not refer to a root folder");
  }

  std::filesystem::path relative_path_value(relative_path);
  if (relative_path_value.is_absolute()) {
    return std::unexpected("Original relative path must not be absolute");
  }

  auto normalized_root_result = Utils::Path::NormalizePath(std::filesystem::path(folder.path));
  if (!normalized_root_result) {
    return std::unexpected("Failed to normalize original root folder path: " +
                           normalized_root_result.error());
  }

  auto normalized_root_path = normalized_root_result.value();
  auto candidate_path = (normalized_root_path / relative_path_value).lexically_normal();
  if (!Utils::Path::IsPathWithinBase(candidate_path, normalized_root_path)) {
    return std::unexpected("Original relative path escapes root folder");
  }

  return candidate_path;
}

}  // namespace Features::Gallery::OriginalLocator
