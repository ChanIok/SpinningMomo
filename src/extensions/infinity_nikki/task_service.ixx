module;

export module Extensions.InfinityNikki.TaskService;

import std;
import Core.State;
import Features.Gallery.Types;
import Extensions.InfinityNikki.Types;

namespace Extensions::InfinityNikki::TaskService {

// 首次注册暖暖相册监听目录后的全量扫描；任务类型 initialScan。调用方：PhotoService 等。
export auto start_initial_scan_task(
    Core::State::AppState& app_state, const Features::Gallery::Types::ScanOptions& options,
    std::function<void(const Features::Gallery::Types::ScanResult&)> post_scan_callback = {})
    -> std::expected<std::string, std::string>;

// 用户显式 / RPC 触发的照片参数解析（会出现在后台任务列表）。同类任务同时只能有一个在
// queued/running。 调用方：extensions RPC、内部再被「按文件夹解析」封装。
export auto start_extract_photo_params_task(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::string, std::string>;

// 图库扫描回调触发的自动解析：不 create_task、不发 task.updated，只打
// Logger；行为上接近硬链接增量同步。
// 若当前已有同类型的用户解析任务（extractPhotoParams）在跑，则跳过，避免与显式任务叠跑。
// 调用方：PhotoService::on_gallery_scan_complete。
export auto schedule_silent_extract_photo_params(
    Core::State::AppState& app_state,
    Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsRequest request) -> void;

// 图库 UI「按文件夹解析」：校验 folder_id、UID 为数字串后，转调 start_extract_photo_params_task。
export auto start_extract_photo_params_for_folder_task(
    Core::State::AppState& app_state,
    const Extensions::InfinityNikki::InfinityNikkiExtractPhotoParamsForFolderRequest& request)
    -> std::expected<std::string, std::string>;

// ScreenShot 目录硬链接初始化（引导 / 设置里的一次性重操作）。任务类型
// initializeScreenshotHardlinks。
export auto start_initialize_screenshot_hardlinks_task(Core::State::AppState& app_state)
    -> std::expected<std::string, std::string>;

}  // namespace Extensions::InfinityNikki::TaskService
