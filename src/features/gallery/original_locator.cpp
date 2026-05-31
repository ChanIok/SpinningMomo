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

// populate 一批 asset 时共用的 folder→root 查找表（不落库，仅内存）。
struct LocatorContext {
  std::unordered_map<std::int64_t, std::int64_t>
      folder_to_root_id;  // 任意 folder.id → 所属监视根 id
  std::unordered_map<std::int64_t, std::filesystem::path>
      root_paths;  // 监视根 id → 规范化后的根路径
};

auto compute_relative_path_string(const std::filesystem::path& asset_path,
                                  const std::filesystem::path& root_path)
    -> std::optional<std::string> {
  if (!Utils::Path::IsPathWithinBase(asset_path, root_path)) {
    return std::nullopt;
  }

  auto relative_path = asset_path.lexically_relative(root_path);
  auto relative_path_string = relative_path.generic_string();
  // 排除空、当前目录、以及逃出 root 的 .. 段
  if (relative_path_string.empty() || relative_path_string == "." ||
      relative_path_string.starts_with("../")) {
    return std::nullopt;
  }

  return relative_path_string;
}

auto build_locator_context(const std::vector<Types::Folder>& folders)
    -> std::expected<LocatorContext, std::string> {
  LocatorContext context;

  // 路径短的先处理，保证子 folder 处理时父 folder 已写入 folder_to_root_id
  std::vector<Types::Folder> sorted_folders = folders;
  std::ranges::sort(sorted_folders, [](const Types::Folder& lhs, const Types::Folder& rhs) {
    if (lhs.path.size() != rhs.path.size()) {
      return lhs.path.size() < rhs.path.size();
    }
    return lhs.path < rhs.path;
  });

  for (const auto& folder : sorted_folders) {
    if (!folder.parent_id.has_value()) {
      // 监视根：自己就是 root_id，并记下规范化根路径
      auto normalized_root_result = Utils::Path::NormalizePath(std::filesystem::path(folder.path));
      if (!normalized_root_result) {
        Logger().error("Failed to lexically normalize watch root folder path '{}': {}", folder.path,
                       normalized_root_result.error());
        continue;
      }

      context.folder_to_root_id[folder.id] = folder.id;
      context.root_paths.emplace(folder.id, normalized_root_result.value());
      continue;
    }

    // 子 folder：继承父 folder 已关联的 root_id
    auto parent_root_it = context.folder_to_root_id.find(folder.parent_id.value());
    if (parent_root_it == context.folder_to_root_id.end()) {
      Logger().error(
          "Folder id={} path='{}' references parent id={} that is not linked to any watch root",
          folder.id, folder.path, folder.parent_id.value());
      continue;
    }

    context.folder_to_root_id.emplace(folder.id, parent_root_it->second);
  }

  return context;
}

auto load_all_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string> {
  auto folders_result = Features::Gallery::Folder::Repository::list_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected("Failed to load folders for original locator: " +
                           folders_result.error());
  }

  return folders_result.value();
}

// 给单个 asset 填 root_id、relative_path（仅改内存，不写库）；失败则字段留空并打日志。
auto try_assign_locator(Core::State::AppState& app_state, const LocatorContext& context,
                        Types::Asset& asset) -> std::expected<void, std::string> {
  asset.root_id.reset();
  asset.relative_path.reset();

  // 用 folder_id 找到监视根
  if (!asset.folder_id.has_value()) {
    Logger().error("Asset id={} path='{}' has no folder_id, cannot derive original locator",
                   asset.id, asset.path);
    return {};
  }

  auto root_it = context.folder_to_root_id.find(asset.folder_id.value());
  if (root_it == context.folder_to_root_id.end()) {
    Logger().error(
        "Asset id={} path='{}' folder_id={} is not linked to any watch root in folder tree",
        asset.id, asset.path, asset.folder_id.value());
    return {};
  }

  const auto root_id = root_it->second;

  // 取监视根的规范化路径，再算 asset 相对路径
  auto root_path_it = context.root_paths.find(root_id);
  if (root_path_it == context.root_paths.end()) {
    Logger().error(
        "Watch root id={} has no lexical path in locator context (asset id={} path='{}')", root_id,
        asset.id, asset.path);
    return {};
  }

  auto normalized_asset_result = Utils::Path::NormalizePath(std::filesystem::path(asset.path));
  if (!normalized_asset_result) {
    Logger().error(
        "Failed to lexically normalize asset path for original locator id={} path='{}': {}",
        asset.id, asset.path, normalized_asset_result.error());
    return {};
  }

  auto relative_path_string =
      compute_relative_path_string(normalized_asset_result.value(), root_path_it->second);
  if (!relative_path_string.has_value()) {
    Logger().error(
        "Asset id={} path='{}' is not under watch root id={} path='{}' according to folder tree",
        asset.id, asset.path, root_id, root_path_it->second.string());
    return {};
  }

  asset.root_id = root_id;
  asset.relative_path = std::move(relative_path_string.value());
  return {};
}

}  // namespace Detail

// WebView 虚拟主机名，与 folder 注册映射一致（如 r-3.test）。
auto make_root_host_name(std::int64_t root_id) -> std::wstring {
  return std::format(L"r-{}.test", root_id);
}

// 列表 RPC 返回前：为每条 asset 补上 root_id、relative_path（整批只查一次 folders）。
auto populate_asset_locators(Core::State::AppState& app_state, std::vector<Types::Asset>& assets)
    -> std::expected<void, std::string> {
  if (assets.empty()) {
    return {};
  }

  // 读 folders 表，建 folder→root 查找表
  auto folders_result = Detail::load_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected(folders_result.error());
  }

  auto context_result = Detail::build_locator_context(folders_result.value());
  if (!context_result) {
    return std::unexpected(context_result.error());
  }

  // 按 folder_id 为每条 asset 算 root_id、relative_path
  for (auto& asset : assets) {
    auto assign_result = Detail::try_assign_locator(app_state, context_result.value(), asset);
    if (!assign_result) {
      return std::unexpected(assign_result.error());
    }
  }

  return {};
}

// 把 URL 里的 root_id + relative_path 拼回磁盘路径（dev 下 /static/assets/originals/by-root/...
// 用）。
auto resolve_original_file_path(Core::State::AppState& app_state, std::int64_t root_id,
                                std::string_view relative_path)
    -> std::expected<std::filesystem::path, std::string> {
  if (root_id <= 0) {
    return std::unexpected("Original root id must be greater than 0");
  }

  if (relative_path.empty()) {
    return std::unexpected("Original relative path is empty");
  }

  // 按 root_id 取监视根 folder 记录
  auto folder_result = Features::Gallery::Folder::Repository::get_folder_by_id(app_state, root_id);
  if (!folder_result) {
    return std::unexpected("Failed to query original root folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    return std::unexpected("Original root folder not found");
  }

  const auto& folder = folder_result->value();
  // root_id 必须对应监视根（parent_id 为空），不能是子 folder
  if (folder.parent_id.has_value()) {
    return std::unexpected("Original root id does not refer to a root folder");
  }

  std::filesystem::path relative_path_value(relative_path);
  if (relative_path_value.is_absolute()) {
    return std::unexpected("Original relative path must not be absolute");
  }

  auto normalized_root_result = Utils::Path::NormalizePath(std::filesystem::path(folder.path));
  if (!normalized_root_result) {
    return std::unexpected("Failed to lexically normalize original root folder path: " +
                           normalized_root_result.error());
  }

  // 根路径 + 相对段拼接（仅 lexical，不访问磁盘）
  auto normalized_root_path = normalized_root_result.value();
  auto candidate_path = std::filesystem::path(
      (normalized_root_path / relative_path_value).lexically_normal().generic_string());
  // 拒绝 .. 等逃出监视根的路径
  if (!Utils::Path::IsPathWithinBase(candidate_path, normalized_root_path)) {
    return std::unexpected("Original relative path escapes root folder");
  }

  return candidate_path;
}

}  // namespace Features::Gallery::OriginalLocator
