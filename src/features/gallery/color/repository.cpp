module;

module Features.Gallery.Color.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.State;
import Core.Database.Types;
import Features.Gallery.Color.Types;
import Features.Gallery.Types;

namespace Features::Gallery::Color::Repository {

auto replace_asset_colors(Core::State::AppState& app_state, std::int64_t asset_id,
                          const std::vector<Types::ExtractedColor>& colors)
    -> std::expected<void, std::string> {
  ColorReplaceBatchItem item{
      .asset_id = asset_id,
      .colors = colors,
  };
  std::vector<ColorReplaceBatchItem> items;
  items.push_back(std::move(item));
  return batch_replace_asset_colors(app_state, items);
}

auto batch_replace_asset_colors(Core::State::AppState& app_state,
                                const std::vector<ColorReplaceBatchItem>& items)
    -> std::expected<void, std::string> {
  if (items.empty()) {
    return {};
  }

  return Core::Database::execute_transaction(
      *app_state.database,
      [&items](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        static const std::string kDeleteSql = "DELETE FROM asset_colors WHERE asset_id = ?";
        static const std::string kInsertSql = R"(
          INSERT INTO asset_colors (
            asset_id, r, g, b, lab_l, lab_a, lab_b, weight, l_bin, a_bin, b_bin
          ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        for (const auto& item : items) {
          if (item.asset_id <= 0) {
            return std::unexpected("Invalid asset_id in color replacement batch");
          }

          auto delete_result = Core::Database::execute(
              db_state, kDeleteSql, std::vector<Core::Database::Types::DbParam>{item.asset_id});
          if (!delete_result) {
            return std::unexpected("Failed to delete existing asset colors for asset_id " +
                                   std::to_string(item.asset_id) + ": " + delete_result.error());
          }

          for (const auto& color : item.colors) {
            std::vector<Core::Database::Types::DbParam> params = {
                item.asset_id,
                static_cast<int64_t>(color.r),
                static_cast<int64_t>(color.g),
                static_cast<int64_t>(color.b),
                static_cast<double>(color.lab_l),
                static_cast<double>(color.lab_a),
                static_cast<double>(color.lab_b),
                static_cast<double>(color.weight),
                static_cast<int64_t>(color.l_bin),
                static_cast<int64_t>(color.a_bin),
                static_cast<int64_t>(color.b_bin),
            };

            auto insert_result = Core::Database::execute(db_state, kInsertSql, params);
            if (!insert_result) {
              return std::unexpected("Failed to insert asset color for asset_id " +
                                     std::to_string(item.asset_id) + ": " + insert_result.error());
            }
          }
        }

        return {};
      });
}

auto get_asset_main_colors(Core::State::AppState& app_state, std::int64_t asset_id)
    -> std::expected<std::vector<Features::Gallery::Types::AssetMainColor>, std::string> {
  if (asset_id <= 0) {
    return std::unexpected("Invalid asset_id");
  }

  static const std::string kQuerySql = R"(
    SELECT r, g, b, weight
    FROM asset_colors
    WHERE asset_id = ?
    ORDER BY weight DESC, id ASC
  )";

  auto result = Core::Database::query<Features::Gallery::Types::AssetMainColor>(
      *app_state.database, kQuerySql, std::vector<Core::Database::Types::DbParam>{asset_id});
  if (!result) {
    return std::unexpected("Failed to query asset main colors: " + result.error());
  }

  return result.value();
}

}  // namespace Features::Gallery::Color::Repository
