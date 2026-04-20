module;

export module Features.Gallery.Watcher;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Watcher {

// 从数据库恢复根目录 watcher 注册信息，但不立即启动监听线程。
export auto restore_watchers_from_db(Core::State::AppState& app_state)
    -> std::expected<void, std::string>;

// 注册一个根目录 watcher（重复调用会更新扫描参数，但不会立即启动线程）。
export auto register_watcher_for_directory(
    Core::State::AppState& app_state, const std::filesystem::path& root_directory,
    const std::optional<Types::ScanOptions>& scan_options = std::nullopt)
    -> std::expected<void, std::string>;

// 为已注册的 watcher 设置扫描完成回调。
export auto set_post_scan_callback_for_directory(
    Core::State::AppState& app_state, const std::filesystem::path& root_directory,
    std::function<void(const Types::ScanResult&)> post_scan_callback)
    -> std::expected<void, std::string>;

// 启动一个已注册的 watcher，可选是否在启动后立即全量扫描一次。
export auto start_watcher_for_directory(Core::State::AppState& app_state,
                                        const std::filesystem::path& root_directory,
                                        bool bootstrap_full_scan = true)
    -> std::expected<void, std::string>;

// 启动所有已注册的 watcher，并在启动后补做一次全量扫描。
export auto start_registered_watchers(Core::State::AppState& app_state)
    -> std::expected<void, std::string>;

// 启动后在后台线程中恢复并启动所有已注册 watcher。
export auto schedule_start_registered_watchers(Core::State::AppState& app_state) -> void;

// 等待后台 watcher 启动恢复任务结束。
export auto wait_for_start_registered_watchers(Core::State::AppState& app_state) -> void;

// 停止并移除某个目录 watcher。返回 true 表示实际移除了 watcher。
export auto remove_watcher_for_directory(Core::State::AppState& app_state,
                                         const std::filesystem::path& root_directory)
    -> std::expected<bool, std::string>;

// 退出时停掉所有 watcher 线程。
export auto shutdown_watchers(Core::State::AppState& app_state) -> void;

// 标记某条手动 move 的源/目标路径进入 watcher 忽略集合（in-flight 阶段）。
export auto begin_manual_move_ignore(Core::State::AppState& app_state,
                                     const std::filesystem::path& source_path,
                                     const std::filesystem::path& destination_path)
    -> std::expected<void, std::string>;

// 手动 move 完成后结束 in-flight，并保留一段短缓冲，吸收延迟到达的目录通知。
export auto complete_manual_move_ignore(Core::State::AppState& app_state,
                                        const std::filesystem::path& source_path,
                                        const std::filesystem::path& destination_path)
    -> std::expected<void, std::string>;

// 将手动文件操作产出的 ScanChange 分发到对应 root watcher 的 post_scan_callback。
export auto dispatch_manual_scan_changes(Core::State::AppState& app_state,
                                         const std::vector<Types::ScanChange>& changes)
    -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Watcher
