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
import <rfl.hpp>;

namespace Features::Gallery::Asset::Repository {

auto create_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO assets (
                name, path, type,
                description, width, height, size, extension, mime_type, hash, folder_id,
                file_created_at, file_modified_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to insert asset item: " + result.error());
  }

  // 获取插入的 ID
  auto id_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, "SELECT last_insert_rowid()");
  if (!id_result) {
    return std::unexpected("Failed to get inserted ID: " + id_result.error());
  }

  return id_result->value_or(0);
}

auto get_asset_by_id(Core::State::AppState& app_state, int64_t id)
    -> std::expected<std::optional<Types::Asset>, std::string> {
  std::string sql = R"(
            SELECT id, name, path, type,
                   description, width, height, size, extension, mime_type, hash, folder_id,
                   file_created_at, file_modified_at,
                   created_at, updated_at
            FROM assets
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::query_single<Types::Asset>(*app_state.database, sql, params);

  if (!result) {
    return std::unexpected("Failed to get asset by id: " + result.error());
  }

  return result.value();
}

auto get_asset_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Asset>, std::string> {
  std::string sql = R"(
            SELECT id, name, path, type,
                   description, width, height, size, extension, mime_type, hash, folder_id,
                   file_created_at, file_modified_at,
                   created_at, updated_at
            FROM assets
            WHERE path = ?
        )";

  std::vector<Core::Database::Types::DbParam> params = {path};

  auto result = Core::Database::query_single<Types::Asset>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query asset item by path: " + result.error());
  }

  return result.value();
}

auto update_asset(Core::State::AppState& app_state, const Types::Asset& item)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE assets SET
                name = ?, path = ?, type = ?,
                description = ?, width = ?, height = ?, size = ?, extension = ?, mime_type = ?, hash = ?, folder_id = ?,
                file_created_at = ?, file_modified_at = ?
            WHERE id = ?
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

  params.push_back(item.id);

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to update asset item: " + result.error());
  }

  return {};
}

auto delete_asset(Core::State::AppState& app_state, int64_t id)
    -> std::expected<void, std::string> {
  std::string sql = "DELETE FROM assets WHERE id = ?";
  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to delete asset item: " + result.error());
  }

  return {};
}

auto batch_create_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<std::vector<std::int64_t>, std::string> {
  if (items.empty()) {
    return std::vector<int64_t>{};
  }

  std::string insert_prefix = R"(
    INSERT INTO assets (
      name, path, type,
      description, width, height, size, extension, mime_type, hash, folder_id,
      file_created_at, file_modified_at
    ) VALUES 
  )";

  std::string values_placeholder = "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  // 参数提取器，将Asset对象转换为参数列表
  auto param_extractor =
      [](const Types::Asset& item) -> std::vector<Core::Database::Types::DbParam> {
    std::vector<Core::Database::Types::DbParam> params;
    params.reserve(13);  // 13个字段

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

    return params;
  };

  // 使用批量插入接口，自动处理分批
  return Core::Database::execute_batch_insert(*app_state.database, insert_prefix,
                                              values_placeholder, items, param_extractor);
}

auto batch_update_asset(Core::State::AppState& app_state, const std::vector<Types::Asset>& items)
    -> std::expected<void, std::string> {
  if (items.empty()) {
    return {};
  }

  // 使用事务批量执行update（SQLite不支持bulk update）
  return Core::Database::execute_transaction(
      *app_state.database,
      [&](Core::Database::State::DatabaseState& db_state) -> std::expected<void, std::string> {
        std::string sql = R"(
          UPDATE assets SET
            name = ?, path = ?, type = ?,
            description = ?, width = ?, height = ?, size = ?, extension = ?, mime_type = ?, hash = ?, folder_id = ?,
            file_created_at = ?, file_modified_at = ?
          WHERE id = ?
        )";

        // 提取参数的lambda，避免在循环中重复代码
        auto extract_params = [](const Types::Asset& item) {
          std::vector<Core::Database::Types::DbParam> params;
          params.reserve(14);  // 13个更新字段 + 1个WHERE条件

          params.push_back(item.name);
          params.push_back(item.path);
          params.push_back(item.type);

          params.push_back(item.description.has_value()
                               ? Core::Database::Types::DbParam{item.description.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(
              item.width.has_value()
                  ? Core::Database::Types::DbParam{static_cast<int64_t>(item.width.value())}
                  : Core::Database::Types::DbParam{std::monostate{}});
          params.push_back(
              item.height.has_value()
                  ? Core::Database::Types::DbParam{static_cast<int64_t>(item.height.value())}
                  : Core::Database::Types::DbParam{std::monostate{}});
          params.push_back(item.size.has_value()
                               ? Core::Database::Types::DbParam{item.size.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.extension.has_value()
                               ? Core::Database::Types::DbParam{item.extension.value()}
                               : Core::Database::Types::DbParam{std::monostate{}});

          params.push_back(item.mime_type);

          params.push_back(item.hash.has_value()
                               ? Core::Database::Types::DbParam{item.hash.value()}
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

          params.push_back(item.id);  // WHERE id = ?

          return params;
        };

        // 执行批量更新
        for (const auto& item : items) {
          auto params = extract_params(item);
          auto result = Core::Database::execute(db_state, sql, params);
          if (!result) {
            return std::unexpected("Failed to update asset item (id=" + std::to_string(item.id) +
                                   "): " + result.error());
          }
        }

        return {};
      });
}

}  // namespace Features::Gallery::Asset::Repository
