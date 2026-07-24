#pragma once

#include "core/database/types.hpp"
#include "features/gallery/types.hpp"

namespace Features::Gallery::Color::Filter {

auto append_color_filter_conditions(const Features::Gallery::Types::QueryAssetsFilters& filters,
                                    std::vector<std::string>& conditions,
                                    std::vector<Core::Database::Types::DbParam>& params,
                                    std::string_view asset_table_alias = "")
    -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Color::Filter
