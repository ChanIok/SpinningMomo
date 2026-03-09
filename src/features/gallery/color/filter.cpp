module;

module Features.Gallery.Color.Filter;

import std;
import Core.Database.Types;
import Features.Gallery.Types;
import Features.Gallery.Color.Extractor;

namespace Features::Gallery::Color::Filter {

constexpr double kDefaultColorDistance = 18.0;
constexpr int kColorBinTolerance = 1;

struct QueryColorTarget {
  float lab_l = 0.0f;
  float lab_a = 0.0f;
  float lab_b = 0.0f;
  int l_bin = 0;
  int a_bin = 0;
  int b_bin = 0;
};

auto build_color_target(const std::string& hex) -> std::expected<QueryColorTarget, std::string> {
  auto rgb_result = Features::Gallery::Color::Extractor::parse_hex_color(hex);
  if (!rgb_result) {
    return std::unexpected(rgb_result.error());
  }

  auto rgb = rgb_result.value();
  auto lab = Features::Gallery::Color::Extractor::rgb_to_lab_color(rgb[0], rgb[1], rgb[2]);
  return QueryColorTarget{
      .lab_l = lab.l,
      .lab_a = lab.a,
      .lab_b = lab.b,
      .l_bin = lab.l_bin,
      .a_bin = lab.a_bin,
      .b_bin = lab.b_bin,
  };
}

auto append_color_match_params(std::vector<Core::Database::Types::DbParam>& params,
                               const QueryColorTarget& target, double distance) -> void {
  params.push_back(std::max(0, target.l_bin - kColorBinTolerance));
  params.push_back(target.l_bin + kColorBinTolerance);
  params.push_back(std::max(0, target.a_bin - kColorBinTolerance));
  params.push_back(target.a_bin + kColorBinTolerance);
  params.push_back(std::max(0, target.b_bin - kColorBinTolerance));
  params.push_back(target.b_bin + kColorBinTolerance);

  params.push_back(static_cast<double>(target.lab_l));
  params.push_back(static_cast<double>(target.lab_l));
  params.push_back(static_cast<double>(target.lab_a));
  params.push_back(static_cast<double>(target.lab_a));
  params.push_back(static_cast<double>(target.lab_b));
  params.push_back(static_cast<double>(target.lab_b));
  params.push_back(distance * distance);
}

auto build_single_color_match_sql() -> std::string {
  return R"(
ac.l_bin BETWEEN ? AND ?
AND ac.a_bin BETWEEN ? AND ?
AND ac.b_bin BETWEEN ? AND ?
AND (
  (ac.lab_l - ?) * (ac.lab_l - ?)
  + (ac.lab_a - ?) * (ac.lab_a - ?)
  + (ac.lab_b - ?) * (ac.lab_b - ?)
) <= ?
)";
}

auto qualify_asset_id(std::string_view asset_table_alias) -> std::string {
  if (asset_table_alias.empty()) {
    return "id";
  }

  return std::string(asset_table_alias) + ".id";
}

auto append_color_filter_conditions(const Features::Gallery::Types::QueryAssetsFilters& filters,
                                    std::vector<std::string>& conditions,
                                    std::vector<Core::Database::Types::DbParam>& params,
                                    std::string_view asset_table_alias)
    -> std::expected<void, std::string> {
  if (!filters.color_hexes.has_value() || filters.color_hexes->empty()) {
    return {};
  }

  std::vector<QueryColorTarget> targets;
  targets.reserve(filters.color_hexes->size());

  for (const auto& color_hex : filters.color_hexes.value()) {
    auto target_result = build_color_target(color_hex);
    if (!target_result) {
      return std::unexpected("Invalid color hex '" + color_hex + "': " + target_result.error());
    }
    targets.push_back(target_result.value());
  }

  const double color_distance =
      std::max(0.1, filters.color_distance.value_or(kDefaultColorDistance));
  const std::string color_match_sql = build_single_color_match_sql();
  const std::string color_match_mode = filters.color_match_mode.value_or("any");
  const auto asset_id = qualify_asset_id(asset_table_alias);

  if (color_match_mode == "all") {
    for (const auto& target : targets) {
      conditions.push_back(std::format("{} IN (SELECT ac.asset_id FROM asset_colors ac WHERE {})",
                                       asset_id, color_match_sql));
      append_color_match_params(params, target, color_distance);
    }
    return {};
  }

  std::vector<std::string> any_match_sql_parts;
  any_match_sql_parts.reserve(targets.size());
  for (const auto& target : targets) {
    any_match_sql_parts.push_back(std::format("({})", color_match_sql));
    append_color_match_params(params, target, color_distance);
  }

  auto merged_any_sql = std::ranges::fold_left(any_match_sql_parts, std::string{},
                                               [](const std::string& acc, const std::string& item) {
                                                 return acc.empty() ? item : acc + " OR " + item;
                                               });

  conditions.push_back(
      std::format("{} IN (SELECT DISTINCT ac.asset_id FROM asset_colors ac WHERE {})", asset_id,
                  merged_any_sql));
  return {};
}

}  // namespace Features::Gallery::Color::Filter
