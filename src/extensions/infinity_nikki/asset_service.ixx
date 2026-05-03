module;

export module Extensions.InfinityNikki.AssetService;

import std;
import Core.State;
import Features.Gallery.Types;
import Extensions.InfinityNikki.Types;
import <asio.hpp>;

namespace Extensions::InfinityNikki::AssetService {

export auto query_photo_map_points(Core::State::AppState& app_state,
                                   const QueryPhotoMapPointsParams& params)
    -> asio::awaitable<std::expected<std::vector<PhotoMapPoint>, std::string>>;

export auto get_details(Core::State::AppState& app_state,
                        const GetInfinityNikkiDetailsParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiDetails, std::string>>;

export auto get_map_config(Core::State::AppState& app_state)
    -> asio::awaitable<std::expected<InfinityNikkiMapConfig, std::string>>;

export auto get_metadata_names(Core::State::AppState& app_state,
                               const GetInfinityNikkiMetadataNamesParams& params)
    -> asio::awaitable<std::expected<InfinityNikkiMetadataNames, std::string>>;

export auto set_user_record(Core::State::AppState& app_state,
                            const SetInfinityNikkiUserRecordParams& params)
    -> std::expected<Features::Gallery::Types::OperationResult, std::string>;

export auto preview_same_outfit_dye_code_fill(
    Core::State::AppState& app_state, const PreviewInfinityNikkiSameOutfitDyeCodeFillParams& params)
    -> std::expected<InfinityNikkiSameOutfitDyeCodeFillPreview, std::string>;

export auto fill_same_outfit_dye_code(Core::State::AppState& app_state,
                                      const FillInfinityNikkiSameOutfitDyeCodeParams& params)
    -> std::expected<InfinityNikkiSameOutfitDyeCodeFillResult, std::string>;

export auto set_world_record(Core::State::AppState& app_state,
                             const SetInfinityNikkiWorldRecordParams& params)
    -> std::expected<Features::Gallery::Types::OperationResult, std::string>;

}  // namespace Extensions::InfinityNikki::AssetService
