#pragma once

#include "vendor/std.hpp"

#include "core/database/types.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::color::filter {

auto append_color_filter_conditions(const features::gallery::QueryAssetsFilters& filters,
                                    std::vector<std::string>& conditions,
                                    std::vector<core::database::DbParam>& params,
                                    std::string_view asset_table_alias = "")
    -> std::expected<void, std::string>;

}  // namespace features::gallery::color::filter
