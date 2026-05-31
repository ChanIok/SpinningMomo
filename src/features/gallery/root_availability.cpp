module;

module Features.Gallery.RootAvailability;

import std;
import Core.Async;
import Core.State;
import Features.Gallery.Folder.Repository;
import Features.Gallery.State;
import Features.Gallery.Types;
import Utils.Logger;
import Utils.Network;
import Utils.Path;
import Utils.String;
import <asio.hpp>;

namespace Features::Gallery::RootAvailability::Detail {

struct ProbeTarget {
  Features::Gallery::Types::Folder folder;
  std::filesystem::path normalized_path;
  std::wstring server;
};

struct ProbeTask {
  ProbeTarget target;
  std::future<Utils::Network::TcpProbeResult> future;
};

auto normalize_root_path(const std::filesystem::path& path)
    -> std::expected<std::filesystem::path, std::string> {
  auto normalized_result = Utils::Path::NormalizePath(path);
  if (!normalized_result) {
    return std::unexpected("Failed to normalize gallery root path: " + normalized_result.error());
  }
  return normalized_result.value();
}

auto store_availability(Core::State::AppState& app_state, std::int64_t root_id,
                        const std::filesystem::path& root_path,
                        Features::Gallery::State::RootAvailability availability) -> void {
  std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
  app_state.gallery->root_availability_by_id[root_id] = availability;
  app_state.gallery->root_availability_by_path[root_path.string()] = availability;
}

}  // namespace Features::Gallery::RootAvailability::Detail

namespace Features::Gallery::RootAvailability {

auto availability_to_string(Features::Gallery::State::RootAvailability availability)
    -> std::string_view {
  switch (availability) {
    case Features::Gallery::State::RootAvailability::Local:
      return "local";
    case Features::Gallery::State::RootAvailability::RemoteReachable:
      return "remote_reachable";
    case Features::Gallery::State::RootAvailability::RemoteUnreachable:
      return "remote_unreachable";
  }
  return "unknown";
}

auto initialize(Core::State::AppState& app_state) -> std::expected<void, std::string> {
  if (!app_state.gallery) {
    return std::unexpected("Gallery state is not initialized");
  }

  auto folders_result = Features::Gallery::Folder::Repository::list_all_folders(app_state);
  if (!folders_result) {
    return std::unexpected("Failed to list folders for root availability: " +
                           folders_result.error());
  }

  {
    std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
    app_state.gallery->root_availability_by_id.clear();
    app_state.gallery->root_availability_by_path.clear();
  }

  std::vector<Detail::ProbeTarget> remote_targets;
  for (const auto& folder : folders_result.value()) {
    if (folder.parent_id.has_value()) {
      continue;
    }

    auto normalized_result = Detail::normalize_root_path(std::filesystem::path(folder.path));
    if (!normalized_result) {
      Logger().warn("Treat gallery root '{}' as unreachable: {}", folder.path,
                    normalized_result.error());
      continue;
    }

    auto normalized_path = normalized_result.value();
    if (Utils::Path::ClassifyPathStorageKind(normalized_path) ==
        Utils::Path::PathStorageKind::Local) {
      Detail::store_availability(app_state, folder.id, normalized_path,
                                 Features::Gallery::State::RootAvailability::Local);
      Logger().debug("Gallery root availability: id={}, path='{}', state=local", folder.id,
                     normalized_path.string());
      continue;
    }

    auto server = Utils::Path::TryParseUncServer(normalized_path);
    if (!server) {
      Detail::store_availability(app_state, folder.id, normalized_path,
                                 Features::Gallery::State::RootAvailability::RemoteUnreachable);
      Logger().warn("Gallery remote root has invalid UNC server: id={}, path='{}'", folder.id,
                    normalized_path.string());
      continue;
    }

    remote_targets.push_back(Detail::ProbeTarget{
        .folder = folder,
        .normalized_path = normalized_path,
        .server = std::move(*server),
    });
  }

  // UNC 首启只做 server:445 探测，不访问 share/path 本身，避免不可达网络路径卡住启动。
  auto* io_context = Core::Async::get_io_context(app_state);
  if (!io_context && !remote_targets.empty()) {
    Logger().warn("Async runtime is unavailable; mark {} remote gallery root(s) unreachable",
                  remote_targets.size());
    for (const auto& target : remote_targets) {
      Detail::store_availability(app_state, target.folder.id, target.normalized_path,
                                 Features::Gallery::State::RootAvailability::RemoteUnreachable);
    }
    remote_targets.clear();
  }

  std::vector<Detail::ProbeTask> probe_tasks;
  probe_tasks.reserve(remote_targets.size());
  for (auto& target : remote_targets) {
    auto server = target.server;
    // 每个 UNC root 独立 co_spawn，使多个离线目标的等待时间接近单个 probe 超时。
    auto future = asio::co_spawn(
        *io_context,
        [server = std::move(server)]() -> asio::awaitable<Utils::Network::TcpProbeResult> {
          co_return co_await Utils::Network::probe_tcp_port(server, L"445",
                                                            kDefaultRemoteProbeTimeout);
        },
        asio::use_future);

    probe_tasks.push_back(Detail::ProbeTask{
        .target = std::move(target),
        .future = std::move(future),
    });
  }

  for (auto& task : probe_tasks) {
    auto probe_result = task.future.get();
    const auto& target = task.target;
    auto availability = probe_result.reachable
                            ? Features::Gallery::State::RootAvailability::RemoteReachable
                            : Features::Gallery::State::RootAvailability::RemoteUnreachable;

    Detail::store_availability(app_state, target.folder.id, target.normalized_path, availability);

    if (probe_result.reachable) {
      Logger().info("Gallery remote root reachable: id={}, server='{}', path='{}'",
                    target.folder.id, Utils::String::ToUtf8(target.server),
                    target.normalized_path.string());
    } else {
      Logger().warn("Gallery remote root unreachable: id={}, server='{}', path='{}', reason='{}'",
                    target.folder.id, Utils::String::ToUtf8(target.server),
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
          return pair.second == Features::Gallery::State::RootAvailability::RemoteUnreachable;
        }));
  }

  Logger().info(
      "Gallery root availability initialized: local_or_reachable={}, remote_unreachable={}",
      total_roots - remote_unreachable, remote_unreachable);
  return {};
}

auto get_for_root_id(Core::State::AppState& app_state, std::int64_t root_id)
    -> std::optional<Features::Gallery::State::RootAvailability> {
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

auto get_for_path(Core::State::AppState& app_state, const std::filesystem::path& root_path)
    -> Features::Gallery::State::RootAvailability {
  auto normalized_result = Detail::normalize_root_path(root_path);
  if (!normalized_result) {
    return Features::Gallery::State::RootAvailability::RemoteUnreachable;
  }

  if (Utils::Path::ClassifyPathStorageKind(normalized_result.value()) ==
      Utils::Path::PathStorageKind::Local) {
    return Features::Gallery::State::RootAvailability::Local;
  }

  if (!app_state.gallery) {
    return Features::Gallery::State::RootAvailability::RemoteUnreachable;
  }

  std::lock_guard<std::mutex> lock(app_state.gallery->root_availability_mutex);
  auto it = app_state.gallery->root_availability_by_path.find(normalized_result->string());
  if (it == app_state.gallery->root_availability_by_path.end()) {
    return Features::Gallery::State::RootAvailability::RemoteReachable;
  }
  return it->second;
}

auto is_remote_unreachable(Core::State::AppState& app_state, std::int64_t root_id) -> bool {
  auto availability = get_for_root_id(app_state, root_id);
  return availability.has_value() &&
         *availability == Features::Gallery::State::RootAvailability::RemoteUnreachable;
}

auto is_remote_unreachable(Core::State::AppState& app_state, const std::filesystem::path& root_path)
    -> bool {
  return get_for_path(app_state, root_path) ==
         Features::Gallery::State::RootAvailability::RemoteUnreachable;
}

}  // namespace Features::Gallery::RootAvailability
