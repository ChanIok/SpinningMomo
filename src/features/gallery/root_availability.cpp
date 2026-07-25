#include "features/gallery/root_availability.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/async/async.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/folder/repository.hpp"
#include "features/gallery/state.hpp"
#include "features/gallery/types.hpp"
#include "utils/logger/logger.hpp"
#include "utils/network/network.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"

namespace features::gallery::root_availability::detail {

struct ProbeTarget {
  features::gallery::Folder folder;
  std::filesystem::path normalized_path;
  std::wstring server;
};

struct ProbeTask {
  ProbeTarget target;
  std::future<utils::network::TcpProbeResult> future;
};

auto normalize_root_path(const std::filesystem::path& path)
    -> std::expected<std::filesystem::path, std::string> {
  auto normalized_result = utils::path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize gallery root path: " + normalized_result.error());
  }
  return normalized_result.value();
}

auto store_availability(core::AppState& app_state, std::int64_t root_id,
                        const std::filesystem::path& root_path,
                        features::gallery::RootAvailability availability) -> void {
  std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
  app_state.gallery->root_availability_by_id[root_id] = availability;
  app_state.gallery->root_availability_by_path[root_path.string()] = availability;
}

}  // namespace features::gallery::root_availability::detail

namespace features::gallery::root_availability {

auto availability_to_string(features::gallery::RootAvailability availability) -> std::string_view {
  switch (availability) {
    case features::gallery::RootAvailability::Local:
      return "local";
    case features::gallery::RootAvailability::RemoteReachable:
      return "remote_reachable";
    case features::gallery::RootAvailability::RemoteUnreachable:
      return "remote_unreachable";
  }
  return "unknown";
}

auto initialize(core::AppState& app_state) -> std::expected<void, std::string> {
  if (!app_state.gallery) {
    return std::unexpected("Gallery state is not initialized");
  }

  auto folders_result = features::gallery::folder::repository::list_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected("Failed to list folders for root availability: " +
                           folders_result.error());
  }

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
    app_state.gallery->root_availability_by_id.clear();
    app_state.gallery->root_availability_by_path.clear();
  }

  std::vector<detail::ProbeTarget> remote_targets;
  for (const auto& folder : folders_result.value()) {
    if (folder.parent_id.has_value()) {
      continue;
    }

    auto normalized_result = detail::normalize_root_path(std::filesystem::path(folder.path));
    if (!normalized_result) {
      Logger().warn("Treat gallery root '{}' as unreachable: {}", folder.path,
                    normalized_result.error());
      continue;
    }

    auto normalized_path = normalized_result.value();
    if (utils::path::ClassifyPathStorageKind(normalized_path) ==
        utils::path::PathStorageKind::Local) {
      detail::store_availability(app_state, folder.id, normalized_path,
                                 features::gallery::RootAvailability::Local);
      Logger().debug("Gallery root availability: id={}, path='{}', state=local", folder.id,
                     normalized_path.string());
      continue;
    }

    auto server = utils::path::TryParseUncServer(normalized_path);
    if (!server) {
      detail::store_availability(app_state, folder.id, normalized_path,
                                 features::gallery::RootAvailability::RemoteUnreachable);
      Logger().warn("Gallery remote root has invalid UNC server: id={}, path='{}'", folder.id,
                    normalized_path.string());
      continue;
    }

    remote_targets.push_back(detail::ProbeTarget{
        .folder = folder,
        .normalized_path = normalized_path,
        .server = std::move(*server),
    });
  }

  // UNC 首启只做 server:445 探测，不访问 share/path 本身，避免不可达网络路径卡住启动。
  auto* io_context = core::async::get_io_context(app_state);
  if (!io_context && !remote_targets.empty()) {
    Logger().warn("Async runtime is unavailable; mark {} remote gallery root(s) unreachable",
                  remote_targets.size());
    for (const auto& target : remote_targets) {
      detail::store_availability(app_state, target.folder.id, target.normalized_path,
                                 features::gallery::RootAvailability::RemoteUnreachable);
    }
    remote_targets.clear();
  }

  std::vector<detail::ProbeTask> probe_tasks;
  probe_tasks.reserve(remote_targets.size());
  for (auto& target : remote_targets) {
    auto server = target.server;
    // 每个 UNC root 独立 co_spawn，使多个离线目标的等待时间接近单个 probe 超时。
    auto future = asio::co_spawn(
        *io_context,
        [server = std::move(server)]() -> asio::awaitable<utils::network::TcpProbeResult> {
          co_return co_await utils::network::probe_tcp_port(server, L"445",
                                                            kDefaultRemoteProbeTimeout);
        },
        asio::use_future);

    probe_tasks.push_back(detail::ProbeTask{
        .target = std::move(target),
        .future = std::move(future),
    });
  }

  for (auto& task : probe_tasks) {
    auto probe_result = task.future.get();
    const auto& target = task.target;
    auto availability = probe_result.reachable
                            ? features::gallery::RootAvailability::RemoteReachable
                            : features::gallery::RootAvailability::RemoteUnreachable;

    detail::store_availability(app_state, target.folder.id, target.normalized_path, availability);

    if (probe_result.reachable) {
      Logger().info("Gallery remote root reachable: id={}, server='{}', path='{}'",
                    target.folder.id, utils::string::ToUtf8(target.server),
                    target.normalized_path.string());
    } else {
      Logger().warn("Gallery remote root unreachable: id={}, server='{}', path='{}', reason='{}'",
                    target.folder.id, utils::string::ToUtf8(target.server),
                    target.normalized_path.string(), probe_result.reason);
    }
  }

  std::size_t total_roots = 0;
  std::size_t remote_unreachable = 0;
  {
    std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
    total_roots = app_state.gallery->root_availability_by_id.size();
    remote_unreachable = static_cast<std::size_t>(
        std::ranges::count_if(app_state.gallery->root_availability_by_id, [](const auto& pair) {
          return pair.second == features::gallery::RootAvailability::RemoteUnreachable;
        }));
  }

  Logger().info(
      "Gallery root availability initialized: local_or_reachable={}, remote_unreachable={}",
      total_roots - remote_unreachable, remote_unreachable);
  return {};
}

auto get_for_root_id(core::AppState& app_state, std::int64_t root_id)
    -> std::optional<features::gallery::RootAvailability> {
  if (!app_state.gallery) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
  auto it = app_state.gallery->root_availability_by_id.find(root_id);
  if (it == app_state.gallery->root_availability_by_id.end()) {
    return std::nullopt;
  }
  return it->second;
}

auto get_for_path(core::AppState& app_state, const std::filesystem::path& root_path)
    -> features::gallery::RootAvailability {
  auto normalized_result = detail::normalize_root_path(root_path);
  if (!normalized_result) {
    return features::gallery::RootAvailability::RemoteUnreachable;
  }

  if (utils::path::ClassifyPathStorageKind(normalized_result.value()) ==
      utils::path::PathStorageKind::Local) {
    return features::gallery::RootAvailability::Local;
  }

  if (!app_state.gallery) {
    return features::gallery::RootAvailability::RemoteUnreachable;
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
  auto it = app_state.gallery->root_availability_by_path.find(normalized_result->string());
  if (it == app_state.gallery->root_availability_by_path.end()) {
    return features::gallery::RootAvailability::RemoteReachable;
  }
  return it->second;
}

auto is_remote_unreachable(core::AppState& app_state, std::int64_t root_id) -> bool {
  auto availability = get_for_root_id(app_state, root_id);
  return availability.has_value() &&
         *availability == features::gallery::RootAvailability::RemoteUnreachable;
}

auto is_remote_unreachable(core::AppState& app_state, const std::filesystem::path& root_path)
    -> bool {
  return get_for_path(app_state, root_path) ==
         features::gallery::RootAvailability::RemoteUnreachable;
}

}  // namespace features::gallery::root_availability
