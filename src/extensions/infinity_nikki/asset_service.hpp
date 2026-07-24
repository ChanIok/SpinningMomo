#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/state/app_state.hpp"
#include "extensions/infinity_nikki/types.hpp"
#include "features/gallery/types.hpp"

namespace extensions::infinity_nikki::asset_service {

auto query_photo_map_points(core::AppState& app_state, const QueryPhotoMapPointsParams& params)
    -> asio::awaitable<std::expected<std::vector<PhotoMapPoint>, std::string>>;

auto get_details(core::AppState& app_state, const GetInfinityNikkiDetailsParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiDetails, std::string>>;

auto get_dye_code_asset_ids(core::AppState& app_state, const GetDyeCodeAssetIdsParams& params)
    -> std::expected<std::vector<std::int64_t>, std::string>;

auto get_map_config(core::AppState& app_state)
    -> asio::awaitable<std::expected<InfinityNikkiMapConfig, std::string>>;

auto get_metadata_names(core::AppState& app_state,
                        const GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiMetadataNames, std::string>>;

auto set_user_record(core::AppState& app_state, const SetInfinityNikkiUserRecordParams& params)
    -> std::expected<features::gallery::OperationResult, std::string>;

auto preview_same_outfit_dye_code_fill(
    core::AppState& app_state, const PreviewInfinityNikkiSameOutfitDyeCodeFillParams& params)
    -> std::expected<InfinityNikkiSameOutfitDyeCodeFillPreview, std::string>;

auto fill_same_outfit_dye_code(core::AppState& app_state,
                               const FillInfinityNikkiSameOutfitDyeCodeParams& params)
    -> std::expected<InfinityNikkiSameOutfitDyeCodeFillResult, std::string>;

auto set_world_record(core::AppState& app_state, const SetInfinityNikkiWorldRecordParams& params)
    -> std::expected<features::gallery::OperationResult, std::string>;

}  // namespace extensions::infinity_nikki::asset_service
