#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "extensions/infinity_nikki/types.hpp"
#include "features/gallery/types.hpp"

namespace extensions::infinity_nikki::task_service {

// 首次注册暖暖相册监听目录后的全量扫描；任务类型 initialScan。调用方：photo_service 等。
auto start_initial_scan_task(
    core::AppState& app_state, const features::gallery::ScanOptions& options,
    std::function<void(const features::gallery::ScanResult&)> post_scan_callback = {})
    -> std::expected<std::string, std::string>;

// 用户显式 / RPC 触发的照片参数解析（会出现在后台任务列表）。同类任务同时只能有一个在
// queued/running。 调用方：extensions RPC、内部再被「按文件夹解析」封装。
auto start_extract_photo_params_task(
    core::AppState& app_state,
    const extensions::infinity_nikki::InfinityNikkiExtractPhotoParamsRequest& request)
    -> std::expected<std::string, std::string>;

// 图库扫描回调触发的自动解析：不 create_task、不发 task.updated，只打
// Logger；行为上接近硬链接增量同步。
// 若当前已有同类型的用户解析任务（extractPhotoParams）在跑，则跳过，避免与显式任务叠跑。
// 调用方：photo_service::on_gallery_scan_complete。
auto schedule_silent_extract_photo_params(
    core::AppState& app_state,
    extensions::infinity_nikki::InfinityNikkiSilentExtractPhotoParamsRequest request) -> void;

// 图库 UI「按文件夹解析」：校验 folder_id、UID 为数字串后，转调 start_extract_photo_params_task。
auto start_extract_photo_params_for_folder_task(
    core::AppState& app_state,
    const extensions::infinity_nikki::InfinityNikkiExtractPhotoParamsForFolderRequest& request)
    -> std::expected<std::string, std::string>;

// 媒体硬链接初始化（引导 / 设置里的一次性重操作）。任务类型
// initializeMediaHardlinks。
auto start_initialize_media_hardlinks_task(core::AppState& app_state)
    -> std::expected<std::string, std::string>;

}  // namespace extensions::infinity_nikki::task_service
