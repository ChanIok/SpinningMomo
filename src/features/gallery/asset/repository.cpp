module;

module Features.Gallery.Asset.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.State;
import Core.Database.Types;
import Features.Gallery.Types;
import Features.Gallery.State;
import Utils.Logger;
import Utils.Time;
import Utils.LRUCache;
import <rfl/json.hpp>;

namespace Features::Gallery::Asset::Repository::Detail {

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

// 提取 Scanner 可写字段，确保批量与单条更新共享同一字段边界。
auto make_scanner_update_params(const Types::Asset& item)
    -> std::vector<Core::Database::Types::DbParam> {
  std::vector<Core::Database::Types::DbParam> params;
  params.reserve(13);

  params.push_back(item.name);
  params.push_back(item.path);
  params.push_back(item.type);
  params.push_back(item.width.has_value()
                       ? Core::Database::Types::DbParam{static_cast<std::int64_t>(*item.width)}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? Core::Database::Types::DbParam{static_cast<std::int64_t>(*item.height)}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.size.has_value() ? Core::Database::Types::DbParam{*item.size}
                                         : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.extension.has_value() ? Core::Database::Types::DbParam{*item.extension}
                                              : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.mime_type);
  params.push_back(item.hash.has_value() ? Core::Database::Types::DbParam{*item.hash}
                                         : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.folder_id.has_value() ? Core::Database::Types::DbParam{*item.folder_id}
                                              : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.file_created_at.has_value()
                       ? Core::Database::Types::DbParam{*item.file_created_at}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.file_modified_at.has_value()
                       ? Core::Database::Types::DbParam{*item.file_modified_at}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.id);
  return params;
}

}  // namespace Features::Gallery::Asset::Repository::Detail

namespace Features::Gallery::Asset::Repository {

auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO assets (
                name, path, type,
                description, width, height, size, extension, mime_type, hash, folder_id,
                file_created_at, file_modified_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            RETURNING id
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(item.name);
  params.push_back(item.path);
  params.push_back(item.type);

  params.push_back(item.description.has_value()
                       ? Core::Database::Types::DbParam{item.description.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.width.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.height.has_value()
                       ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.size.has_value() ? Core::Database::Types::DbParam{item.size.value()}
                                         : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.extension.has_value()
                       ? Core::Database::Types::DbParam{item.extension.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.mime_type);

  params.push_back(item.hash.has_value() ? Core::Database::Types::DbParam{item.hash.value()}
                                         : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.folder_id.has_value()
                       ? Core::Database::Types::DbParam{item.folder_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(item.file_created_at.has_value()
                       ? Core::Database::Types::DbParam{item.file_created_at.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});
  params.push_back(item.file_modified_at.has_value()
                       ? Core::Database::Types::DbParam{item.file_modified_at.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  auto result = Core::Database::query_scalar<std::int64_t>(app_state, sql, params);
  if (!result || !result->has_value()) {
    return std::unexpected("Failed to insert asset item: " +
                           (result ? std::string("missing returned ID") : result.error()));
  }

  return result->value();
}

auto get_asset_by_id(Core::State::AppState& app_state, int64_t id)
    -> std::expected<std::optional<Types::Asset>, std::string> {
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

  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::query_single<Types::Asset>(app_state, sql, params);

  if (!result) {
    return std::unexpected("Failed to get asset by id: " + result.error());
  }

  return result.value();
}

auto get_asset_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Asset>, std::string> {
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

  std::vector<Core::Database::Types::DbParam> params = {path};

  auto result = Core::Database::query_single<Types::Asset>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to query asset item by path: " + result.error());
  }

  return result.value();
}

auto has_assets_under_path_prefix(Core::State::AppState& app_state, const std::string& path_prefix)
    -> std::expected<bool, std::string> {
  // 这里不是查“这个目录本身是否有一条 folder 记录”，
  // 而是查 assets 表里是否已经存在任何文件路径落在该目录下面。
  // 例如 path_prefix = C:/A/B 时，我们要匹配的是 C:/A/B/xxx.jpg。
  auto normalized_prefix = path_prefix;
  if (!normalized_prefix.empty() && normalized_prefix.ends_with('/')) {
    normalized_prefix.pop_back();
  }

  auto escaped_prefix = Detail::escape_like_pattern(normalized_prefix);
  std::string sql = R"(
            SELECT EXISTS(
                SELECT 1
                FROM assets
                WHERE path LIKE ? ESCAPE '\'
            )
        )";

  // 这里拼成 "prefix/%"，只匹配“这个目录的子内容”，
  // 不会把名称相似但不在该目录下的路径误算进去。
  std::vector<Core::Database::Types::DbParam> params = {escaped_prefix + "/%"};

  auto result = Core::Database::query_scalar<std::int64_t>(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to query assets by path prefix: " + result.error());
  }

  return result->value_or(0) != 0;
}

// 只更新 Scanner 从文件系统派生的索引字段，不触碰用户编辑字段。
auto update_asset_scanner_fields(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE assets SET
                name = ?, path = ?, type = ?,
                width = ?, height = ?, size = ?, extension = ?, mime_type = ?, hash = ?, folder_id = ?,
                file_created_at = ?, file_modified_at = ?
            WHERE id = ?
        )";

  auto result = Core::Database::execute(app_state, sql, Detail::make_scanner_update_params(item));
  if (!result) {
    return std::unexpected("Failed to update asset scanner fields: " + result.error());
  }

  return {};
}

// 手动移动文件后只更新位置字段，避免把旧对象中的其他字段顺手写回。
auto update_asset_location(Core::State::AppState& app_state, std::int64_t asset_id,
                           const std::string& name, const std::string& path,
                           std::optional<std::int64_t> folder_id)
    -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE assets
    SET name = ?, path = ?, folder_id = ?
    WHERE id = ?
  )";

  std::vector<Core::Database::Types::DbParam> params = {
      name,
      path,
      folder_id.has_value() ? Core::Database::Types::DbParam{*folder_id}
                            : Core::Database::Types::DbParam{std::monostate{}},
      asset_id,
  };
  auto result = Core::Database::execute(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to update asset location: " + result.error());
  }
  return {};
}

// 同步内容未变资产的文件状态，避免后续扫描重复计算指纹
auto update_asset_file_state(Core::State::AppState& app_state, std::int64_t asset_id,
                             std::int64_t size, std::int64_t file_modified_at)
    -> std::expected<void, std::string> {
  std::string sql = R"(
    UPDATE assets
    SET size = ?, file_modified_at = ?
    WHERE id = ?
  )";

  auto result = Core::Database::execute(app_state, sql, {size, file_modified_at, asset_id});
  if (!result) {
    return std::unexpected("Failed to update asset file state: " + result.error());
  }

  return {};
}

auto delete_asset(Core::State::AppState& app_state, int64_t id)
    -> std::expected<void, std::string> {
  std::string sql = "DELETE FROM assets WHERE id = ?";
  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::execute(app_state, sql, params);
  if (!result) {
    return std::unexpected("Failed to delete asset item: " + result.error());
  }

  return {};
}

auto batch_delete_assets_by_ids(Core::State::AppState& app_state,
                                const std::vector<std::int64_t>& ids)
    -> std::expected<void, std::string> {
  if (ids.empty()) {
    return {};
  }

  std::unordered_set<std::int64_t> unique_ids(ids.begin(), ids.end());
  return Core::Database::execute_transaction(
      app_state,
      [&unique_ids](Core::State::AppState& txn_app_state) -> std::expected<void, std::string> {
        constexpr std::string_view sql = "DELETE FROM assets WHERE id = ?";
        for (auto id : unique_ids) {
          auto result = Core::Database::execute(txn_app_state, std::string(sql), {id});
          if (!result) {
            return std::unexpected("Failed to delete asset item (id=" + std::to_string(id) +
                                   "): " + result.error());
          }
        }

        return {};
      });
}

}  // namespace Features::Gallery::Asset::Repository
