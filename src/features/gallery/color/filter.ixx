module;

export module Features.Gallery.Color.Filter;

import std;
import Core.Database.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Color::Filter {

export auto append_color_filter_conditions(
    const Features::Gallery::Types::QueryAssetsFilters& filters,
    std::vector<std::string>& conditions, std::vector<Core::Database::Types::DbParam>& params,
    std::string_view asset_table_alias = "") -> std::expected<void, std::string>;

}  // namespace Features::Gallery::Color::Filter
