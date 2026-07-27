#include "features/gallery/asset/repository.hpp"

#include "vendor/std.hpp"

#include "vendor/rfl.hpp"

#include "core/database/database.hpp"
#include "core/database/state.hpp"
#include "core/database/types.hpp"
#include "core/state/app_state.hpp"
#include "features/gallery/state.hpp"
#include "features/gallery/types.hpp"
#include "utils/logger/logger.hpp"
#include "utils/lru_cache.hpp"
#include "utils/time.hpp"

namespace features::gallery::asset::repository::detail {

auto escape_like_pattern(const std::string& input) -> std::string {
  // SQLite 的 LIKE 会把 % 和 _ 当成通配符。
  // 这里把路径里的特殊字符转义掉，确保查询按“真实路径文本”匹配，
  // 而不是把目录名误当成模糊匹配规则。
  std::string escaped;
  escaped.reserve(input.size());
  for (char ch : input) {
    if (ch == '\\' || ch == '%' || ch == '_') {
      escaped.push_back('\\');
    }
    escaped.push_back(ch);
  }
  return escaped;
}

// 提取 scanner 可写字段，确保批量与单条更新共享同一字段边界。
auto make_scanner_update_params(const Asset& item) -> std::vector<core::database::DbParam> {
  std::vector<core::database::DbParam> params;
  params.reserve(13);

  params.push_back(item.name);
  params.push_back(item.path);
  params.push_back(item.type);
  params.push_back(item.width.has_value()
                       ? core::database::DbParam{static_cast<std::int64_t>(*item.width)}
                       : core::database::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? core::database::DbParam{static_cast<std::int64_t>(*item.height)}
                       : core::database::DbParam{std::monostate{}});
  params.push_back(item.size.has_value() ? core::database::DbParam{*item.size}
                                         : core::database::DbParam{std::monostate{}});
  params.push_back(item.extension.has_value() ? core::database::DbParam{*item.extension}
                                              : core::database::DbParam{std::monostate{}});
  params.push_back(item.mime_type);
  params.push_back(item.hash.has_value() ? core::database::DbParam{*item.hash}
                                         : core::database::DbParam{std::monostate{}});
  params.push_back(item.folder_id.has_value() ? core::database::DbParam{*item.folder_id}
                                              : core::database::DbParam{std::monostate{}});
  params.push_back(item.file_created_at.has_value() ? core::database::DbParam{*item.file_created_at}
                                                    : core::database::DbParam{std::monostate{}});
  params.push_back(item.file_modified_at.has_value()
                       ? core::database::DbParam{*item.file_modified_at}
                       : core::database::DbParam{std::monostate{}});
  params.push_back(item.id);
  return params;
}

}  // namespace features::gallery::asset::repository::detail

namespace features::gallery::asset::repository {

auto create_asset(core::AppState& app_state, const Asset& item)
    -> std::expected<int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO assets (
                name, path, type,
                description, width, height, size, extension, mime_type, hash, folder_id,
                file_created_at, file_modified_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            RETURNING id
        )";

  std::vector<core::database::DbParam> params;
  params.push_back(item.name);
  params.push_back(item.path);
  params.push_back(item.type);

  params.push_back(item.description.has_value() ? core::database::DbParam{item.description.value()}
                                                : core::database::DbParam{std::monostate{}});

  params.push_back(item.width.has_value()
                       ? core::database::DbParam{static_cast<int64_t>(item.width.value())}
                       : core::database::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? core::database::DbParam{static_cast<int64_t>(item.height.value())}
                       : core::database::DbParam{std::monostate{}});
  params.push_back(item.size.has_value() ? core::database::DbParam{item.size.value()}
                                         : core::database::DbParam{std::monostate{}});

  params.push_back(item.extension.has_value() ? core::database::DbParam{item.extension.value()}
                                              : core::database::DbParam{std::monostate{}});

  params.push_back(item.mime_type);

  params.push_back(item.hash.has_value() ? core::database::DbParam{item.hash.value()}
                                         : core::database::DbParam{std::monostate{}});

  params.push_back(item.folder_id.has_value() ? core::database::DbParam{item.folder_id.value()}
                                              : core::database::DbParam{std::monostate{}});

  params.push_back(item.file_created_at.has_value()
                       ? core::database::DbParam{item.file_created_at.value()}
                       : core::database::DbParam{std::monostate{}});
  params.push_back(item.file_modified_at.has_value()
                       ? core::database::DbParam{item.file_modified_at.value()}
                       : core::database::DbParam{std::monostate{}});

  auto result = core::database::query_scalar<std::int64_t>(app_state, sql, params);
  if (!result || !result->has_value()) {
    return std::unexpected("Failed to insert asset item: " +
                           (result ? std::string("missing returned ID") : result.error()));
  }

  return result->value();
}

auto create_asset_with_inherited_data_in_transaction(core::AppState& app_state, const Asset& item)
    -> std::expected<std::int64_t, std::string> {
  auto create_result = create_asset(app_state, item);
  if (!create_result) {
    return std::unexpected(create_result.error());
  }

  const auto new_asset_id = create_result.value();
  if (!item.hash.has_value() || item.hash->empty()) {
    return new_asset_id;
  }

  auto source_result = core::database::query_scalar<std::int64_t>(
      app_state, "SELECT id FROM assets WHERE hash = ? AND id <> ? ORDER BY id ASC LIMIT 1",
      {*item.hash, new_asset_id});
  if (!source_result) {
    return std::unexpected("Failed to find asset inheritance source: " + source_result.error());
  }
  if (!source_result->has_value()) {
    return new_asset_id;
  }

  const auto source_asset_id = source_result->value();
  auto fields_result =
      core::database::execute(app_state,
                              R"(
        UPDATE assets
        SET description = (SELECT description FROM assets WHERE id = ?),
            rating = (SELECT rating FROM assets WHERE id = ?),
            review_flag = (SELECT review_flag FROM assets WHERE id = ?)
        WHERE id = ?
      )",
                              {source_asset_id, source_asset_id, source_asset_id, new_asset_id});
  if (!fields_result) {
    return std::unexpected("Failed to inherit asset user fields: " + fields_result.error());
  }

  auto tags_result = core::database::execute(app_state,
                                             R"(
        INSERT OR IGNORE INTO asset_tags (asset_id, tag_id)
        SELECT ?, tag_id FROM asset_tags WHERE asset_id = ?
      )",
                                             {new_asset_id, source_asset_id});
  if (!tags_result) {
    return std::unexpected("Failed to inherit asset tags: " + tags_result.error());
  }

  if (app_state.gallery->inherit_asset_data_callback) {
    auto extension_result =
        app_state.gallery->inherit_asset_data_callback(new_asset_id, source_asset_id);
    if (!extension_result) {
      return std::unexpected("Failed to inherit extension asset data: " + extension_result.error());
    }
  }

  return new_asset_id;
}

auto get_asset_by_id(core::AppState& app_state, int64_t id)
    -> std::expected<std::optional<Asset>, std::string> {
  std::string sql = R"(
            SELECT id, name, path, type,
                   NULL AS dominant_color_hex,
                   rating, review_flag,
                   description, width, height, size, extension, mime_type, hash,
                   NULL AS root_id, NULL AS relative_path, folder_id,
                   file_created_at, file_modified_at,
                   created_at, updated_at
            FROM assets
            WHERE id = ?
        )";

  std::vector<core::database::DbParam> params = {id};

  auto result = core::database::query_single<Asset>(app_state, sql, params);

  if (!result) {
    return std::unexpected("Failed to get asset by id: " + result.error());
  }

  return result.value();
}

auto get_asset_by_path(core::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Asset>, std::string> {
  std::string sql = R"(
            SELECT id, name, path, type,
                   NULL AS dominant_color_hex,
                   rating, review_flag,
                   description, width, height, size, extension, mime_type, hash,
                   NULL AS root_id, NULL AS relative_path, folder_id,
                   file_created_at, file_modified_at,
                   created_at, updated_at
            FROM assets
            WHERE path = ?
        )";

  std::vector<core::database::DbParam> params = {path};

  auto result = core::database::query_single<Asset>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to query asset item by path: " + result.error());
  }

  return result.value();
}

auto has_assets_under_path_prefix(core::AppState& app_state, const std::string& path_prefix)
    -> std::expected<bool, std::string> {
  // 这里不是查“这个目录本身是否有一条 folder 记录”，
  // 而是查 assets 表里是否已经存在任何文件路径落在该目录下面。
  // 例如 path_prefix = C:/A/B 时，我们要匹配的是 C:/A/B/xxx.jpg。
  auto normalized_prefix = path_prefix;
  if (!normalized_prefix.empty() && normalized_prefix.ends_with('/')) {
    normalized_prefix.pop_back();
  }

  auto escaped_prefix = detail::escape_like_pattern(normalized_prefix);
  std::string sql = R"(
            SELECT EXISTS(
                SELECT 1
                FROM assets
                WHERE path LIKE ? ESCAPE '\'
            )
        )";

  // 这里拼成 "prefix/%"，只匹配“这个目录的子内容”，
  // 不会把名称相似但不在该目录下的路径误算进去。
  std::vector<core::database::DbParam> params = {escaped_prefix + "/%"};

  auto result = core::database::query_scalar<std::int64_t>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to query assets by path prefix: " + result.error());
  }

  return result->value_or(0) != 0;
}

// 只更新 scanner 从文件系统派生的索引字段，不触碰用户编辑字段。
auto update_asset_scanner_fields(core::AppState& app_state, const Asset& item)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE assets SET
                name = ?, path = ?, type = ?,
                width = ?, height = ?, size = ?, extension = ?, mime_type = ?, hash = ?, folder_id = ?,
                file_created_at = ?, file_modified_at = ?, missing_at = NULL
            WHERE id = ?
        )";

  auto result = core::database::execute(app_state, sql, detail::make_scanner_update_params(item));
  if (!result) {
    return std::unexpected("Failed to update asset scanner fields: " + result.error());
  }

  return {};
}

// 手动移动文件后只更新位置字段，避免把旧对象中的其他字段顺手写回。
auto update_asset_location(core::AppState& app_state, std::int64_t asset_id,
                           const std::string& name, const std::string& path,
                           std::optional<std::int64_t> folder_id)
    -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE assets
    SET name = ?, path = ?, folder_id = ?
    WHERE id = ?
  )";

  std::vector<core::database::DbParam> params = {
      name,
      path,
      folder_id.has_value() ? core::database::DbParam{*folder_id}
                            : core::database::DbParam{std::monostate{}},
      asset_id,
  };
  auto result = core::database::execute(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to update asset location: " + result.error());
  }
  return {};
}

// 同步内容未变资产的文件状态，避免后续扫描重复计算指纹
auto update_asset_file_state(core::AppState& app_state, std::int64_t asset_id, std::int64_t size,
                             std::int64_t file_modified_at) -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE assets
    SET size = ?, file_modified_at = ?, missing_at = NULL
    WHERE id = ?
  )";

  auto result = core::database::execute(app_state, sql, {size, file_modified_at, asset_id});
  if (!result) {
    return std::unexpected("Failed to update asset file state: " + result.error());
  }

  return {};
}

auto mark_asset_missing_by_path(core::AppState& app_state, const std::string& path)
    -> std::expected<bool, std::string> {
  auto result = core::database::query_scalar<std::int64_t>(app_state,
                                                           R"(
        UPDATE assets
        SET missing_at = unixepoch('subsec') * 1000
        WHERE path = ? AND missing_at IS NULL
        RETURNING id
      )",
                                                           {path});
  if (!result) {
    return std::unexpected("Failed to mark asset missing: " + result.error());
  }
  return result->has_value();
}

auto restore_assets_by_ids(core::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<void, std::string> {
  if (ids.empty()) {
    return {};
  }

  return core::database::execute_transaction(
      app_state, [&ids](core::AppState& txn_app_state) -> std::expected<void, std::string> {
        for (const auto id : ids) {
          auto result = core::database::execute(
              txn_app_state, "UPDATE assets SET missing_at = NULL WHERE id = ?", {id});
          if (!result) {
            return std::unexpected("Failed to restore asset (id=" + std::to_string(id) +
                                   "): " + result.error());
          }
        }
        return {};
      });
}

auto restore_asset_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<bool, std::string> {
  auto result = core::database::query_scalar<std::int64_t>(
      app_state,
      "UPDATE assets SET missing_at = NULL WHERE id = ? AND missing_at IS NOT NULL RETURNING id",
      {id});
  if (!result) {
    return std::unexpected("Failed to restore asset: " + result.error());
  }
  return result->has_value();
}

auto purge_expired_missing_assets(core::AppState& app_state, std::int64_t cutoff_millis)
    -> std::expected<std::int64_t, std::string> {
  auto result = core::database::query<core::database::ReturningIdRow>(
      app_state, "DELETE FROM assets WHERE missing_at IS NOT NULL AND missing_at < ? RETURNING id",
      {cutoff_millis});
  if (!result) {
    return std::unexpected("Failed to purge expired missing assets: " + result.error());
  }
  return static_cast<std::int64_t>(result->size());
}

auto batch_delete_assets_by_ids(core::AppState& app_state, const std::vector<std::int64_t>& ids)
    -> std::expected<void, std::string> {
  if (ids.empty()) {
    return {};
  }

  std::unordered_set<std::int64_t> unique_ids(ids.begin(), ids.end());
  return core::database::execute_transaction(
      app_state, [&unique_ids](core::AppState& txn_app_state) -> std::expected<void, std::string> {
        constexpr std::string_view sql = "DELETE FROM assets WHERE id = ?";
        for (auto id : unique_ids) {
          auto result = core::database::execute(txn_app_state, std::string(sql), {id});
          if (!result) {
            return std::unexpected("Failed to delete asset item (id=" + std::to_string(id) +
                                   "): " + result.error());
          }
        }

        return {};
      });
}

}  // namespace features::gallery::asset::repository
