#include "extensions/infinity_nikki/role_profile.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"
#include "vendor/rfl.hpp"

#include "core/async/async.hpp"
#include "core/http_client/http_client.hpp"
#include "core/http_client/types.hpp"
#include "core/rpc/notification_hub.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/folder/repository.hpp"
#include "features/gallery/folder/service.hpp"
#include "features/gallery/types.hpp"
#include "features/settings/state.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"
#include "utils/string/string.hpp"

namespace extensions::infinity_nikki::role_profile {

constexpr std::string_view kRoleApiBaseUrl = "https://nuan5.pro/api/role/";
constexpr std::int32_t kRoleApiConnectTimeoutMs = 3000;
constexpr std::int32_t kRoleApiReceiveTimeoutMs = 5000;
constexpr std::size_t kMaxNicknameBytes = 128;
constexpr auto kRoleApiMinRequestInterval = std::chrono::seconds(2);

struct RoleResponse {
  std::optional<std::string> nickname;
};

struct NicknameSyncState {
  std::mutex mutex;
  std::unordered_set<std::string> pending_uids;
};

// 返回进程内共享的请求去重状态，避免相邻扫描并发查询同一 UID。
auto nickname_sync_state() -> NicknameSyncState& {
  static NicknameSyncState state;
  return state;
}

// 判断新建目录是否是当前 GamePlayPhotos 根下的直属数字 UID 文件夹。
auto is_direct_uid_folder(const features::gallery::Folder& folder,
                          const std::filesystem::path& game_play_photos_root) -> bool {
  if (folder.name.empty() ||
      !std::ranges::all_of(folder.name, [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
    return false;
  }

  auto folder_path_result = utils::path::NormalizePath(folder.path);
  if (!folder_path_result) {
    return false;
  }

  // 只比较直接父目录，避免其他层级中恰好为数字的目录触发账号查询。
  return utils::path::NormalizeForComparison(folder_path_result->parent_path()) ==
         utils::path::NormalizeForComparison(game_play_photos_root);
}

// 解析角色接口响应，并拒绝缺失、空白或异常过长的昵称。
auto parse_role_nickname(const std::string& response_body)
    -> std::expected<std::string, std::string> {
  auto parsed = rfl::json::read<RoleResponse, rfl::DefaultIfMissing>(response_body);
  if (!parsed) {
    return std::unexpected("Failed to parse role response: " + parsed.error().what());
  }
  if (!parsed->nickname.has_value()) {
    return std::unexpected("Role response is missing nickname");
  }

  auto nickname = utils::string::TrimAscii(parsed->nickname.value());
  if (nickname.empty()) {
    return std::unexpected("Role response contains an empty nickname");
  }
  if (nickname.size() > kMaxNicknameBytes) {
    return std::unexpected("Role response nickname is too long");
  }
  return nickname;
}

// 等待全局一秒请求间隔，确保并发批次合计也不会超过接口频率限制。
auto acquire_role_api_send_slot() -> asio::awaitable<void> {
  static std::mutex gate_mutex;
  static auto next_allowed_at = std::chrono::steady_clock::time_point::min();

  auto executor = co_await asio::this_coro::executor;
  asio::steady_timer timer(executor);

  while (true) {
    auto wait_duration = std::chrono::steady_clock::duration::zero();
    {
      std::lock_guard<std::mutex> lock(gate_mutex);
      auto now = std::chrono::steady_clock::now();
      if (now >= next_allowed_at) {
        next_allowed_at = now + kRoleApiMinRequestInterval;
        co_return;
      }
      wait_duration = next_allowed_at - now;
    }

    timer.expires_after(wait_duration);
    std::error_code wait_error;
    co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, wait_error));
  }
}

// 请求指定 UID 的公开角色资料并返回经过校验的昵称。
auto fetch_role_nickname(core::AppState& app_state, const std::string& uid)
    -> asio::awaitable<std::expected<std::string, std::string>> {
  // 所有角色查询共用发送间隔，与现有 NUAN5.PRO 照片解析请求保持同等节制。
  co_await acquire_role_api_send_slot();

  core::http_client::Request request{
      .method = "GET",
      .url = std::string(kRoleApiBaseUrl) + uid,
      .headers =
          {
              core::http_client::Header{.name = "Accept", .value = "application/json"},
              core::http_client::Header{.name = "X-Client", .value = "SpinningMomo"},
          },
      .connect_timeout_ms = kRoleApiConnectTimeoutMs,
      .receive_timeout_ms = kRoleApiReceiveTimeoutMs,
  };

  auto response = co_await core::http_client::fetch(app_state, request);
  if (!response) {
    co_return std::unexpected("Failed to fetch role profile: " + response.error());
  }
  if (response->status_code < 200 || response->status_code >= 300) {
    co_return std::unexpected("Role profile returned non-2xx response: " +
                              std::to_string(response->status_code));
  }

  co_return parse_role_nickname(response->body);
}

// 为一个新 UID 文件夹查询昵称，并仅在它仍无显示名称时写入数据库。
auto sync_nickname_for_folder(core::AppState& app_state,
                              const features::gallery::Folder& created_folder)
    -> asio::awaitable<std::expected<bool, std::string>> {
  auto folder_result =
      features::gallery::folder::repository::get_folder_by_id(app_state, created_folder.id);
  if (!folder_result) {
    co_return std::unexpected("Failed to query UID folder: " + folder_result.error());
  }
  if (!folder_result->has_value()) {
    co_return false;
  }

  const auto& folder = folder_result->value();
  if (folder.display_name.has_value() &&
      !utils::string::TrimAscii(folder.display_name.value()).empty()) {
    // 已有名称可能来自用户手动编辑，跳过网络请求也不改变现有值。
    co_return false;
  }

  auto nickname_result = co_await fetch_role_nickname(app_state, folder.name);
  if (!nickname_result) {
    co_return std::unexpected(nickname_result.error());
  }

  // 条件更新在数据库层再次检查空值，封住请求期间用户手动改名的竞争窗口。
  auto update_result = features::gallery::folder::service::update_folder_display_name_if_empty(
      app_state, folder.id, nickname_result.value());
  if (!update_result) {
    co_return std::unexpected("Failed to save role nickname: " + update_result.error());
  }
  co_return update_result.value();
}

// 释放 UID 的请求占位，结束本次新目录对应的在线查询生命周期。
auto release_pending_uid(const std::string& uid) -> void {
  auto& state = nickname_sync_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.pending_uids.erase(uid);
}

// 从本轮新建目录中批量补全直属 UID 文件夹昵称，并在整批结束后合并通知。
auto schedule_nickname_sync_for_created_folders(
    core::AppState& app_state, const std::filesystem::path& game_play_photos_root,
    const std::vector<features::gallery::Folder>& created_folders) -> void {
  if (!app_state.async || !app_state.settings ||
      !app_state.settings->raw.extensions.infinity_nikki.allow_online_photo_metadata_extract) {
    return;
  }

  std::vector<features::gallery::Folder> pending_folders;
  pending_folders.reserve(created_folders.size());

  // 在投递协程前一次筛选并占住 UID，避免相邻扫描重复查询同一账号。
  auto& sync_state = nickname_sync_state();
  {
    std::lock_guard<std::mutex> lock(sync_state.mutex);
    for (const auto& folder : created_folders) {
      if (is_direct_uid_folder(folder, game_play_photos_root) &&
          sync_state.pending_uids.insert(folder.name).second) {
        pending_folders.push_back(folder);
      }
    }
  }
  if (pending_folders.empty()) {
    return;
  }

  auto* io_context = core::async::get_io_context(app_state);
  if (!io_context) {
    for (const auto& folder : pending_folders) {
      release_pending_uid(folder.name);
    }
    Logger().warn("Skip InfinityNikki role nickname sync: async runtime is unavailable");
    return;
  }

  asio::co_spawn(
      *io_context,
      [&app_state, pending_folders = std::move(pending_folders)]() -> asio::awaitable<void> {
        co_await asio::post(asio::use_awaitable);

        bool updated_any = false;
        for (const auto& folder : pending_folders) {
          // 设置在批次排队期间被关闭时停止剩余请求。
          if (!app_state.settings || !app_state.settings->raw.extensions.infinity_nikki
                                          .allow_online_photo_metadata_extract) {
            break;
          }

          try {
            auto sync_result = co_await sync_nickname_for_folder(app_state, folder);
            if (!sync_result) {
              Logger().warn("InfinityNikki role nickname sync failed for UID {}: {}", folder.name,
                            sync_result.error());
            } else if (sync_result.value()) {
              updated_any = true;
              Logger().info("InfinityNikki role nickname synced for UID {}", folder.name);
            }
          } catch (const std::exception& error) {
            // 单个账号异常只结束本项，批次继续处理其他账号。
            Logger().warn("InfinityNikki role nickname sync threw for UID {}: {}", folder.name,
                          error.what());
          }
        }

        // 无论批次是否中途停用设置，都释放本轮全部 UID 占位。
        for (const auto& folder : pending_folders) {
          release_pending_uid(folder.name);
        }
        if (updated_any) {
          core::rpc::notification_hub::send_notification(app_state, "gallery.changed");
        }
      },
      core::async::log_completion("Infinity Nikki role profile sync"));
}

}  // namespace extensions::infinity_nikki::role_profile
