module;

module Features.Gallery.Folder.Repository;

import std;
import Core.State;
import Core.Database;
import Core.Database.Types;
import Features.Gallery.Types;
import Utils.Logger;
import <rfl.hpp>;

namespace Features::Gallery::Folder::Repository {

auto create_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<std::int64_t, std::string> {
  std::string sql = R"(
            INSERT INTO folders (
                path, parent_id, name, display_name, 
                sort_order, is_hidden
            ) VALUES (?, ?, ?, ?, ?, ?)
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(folder.path);

  params.push_back(folder.parent_id.has_value()
                       ? Core::Database::Types::DbParam{folder.parent_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(folder.name);

  params.push_back(folder.display_name.has_value()
                       ? Core::Database::Types::DbParam{folder.display_name.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(static_cast<int64_t>(folder.sort_order));
  params.push_back(folder.is_hidden);

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to insert folder: " + result.error());
  }

  // 获取插入的 ID
  auto id_result =
      Core::Database::query_scalar<int64_t>(*app_state.database, "SELECT last_insert_rowid()");
  if (!id_result) {
    return std::unexpected("Failed to get inserted folder ID: " + id_result.error());
  }

  return id_result->value_or(0);
}

auto get_folder_by_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::optional<Types::Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE path = ?
        )";

  std::vector<Core::Database::Types::DbParam> params = {path};

  auto result = Core::Database::query_single<Types::Folder>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query folder by path: " + result.error());
  }

  return result.value();
}

auto get_folder_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::query_single<Types::Folder>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to query folder by id: " + result.error());
  }

  return result.value();
}

auto update_folder(Core::State::AppState& app_state, const Types::Folder& folder)
    -> std::expected<void, std::string> {
  std::string sql = R"(
            UPDATE folders SET
                path = ?, parent_id = ?, name = ?, display_name = ?,
                cover_asset_id = ?, sort_order = ?, is_hidden = ?
            WHERE id = ?
        )";

  std::vector<Core::Database::Types::DbParam> params;
  params.push_back(folder.path);

  params.push_back(folder.parent_id.has_value()
                       ? Core::Database::Types::DbParam{folder.parent_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(folder.name);

  params.push_back(folder.display_name.has_value()
                       ? Core::Database::Types::DbParam{folder.display_name.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(folder.cover_asset_id.has_value()
                       ? Core::Database::Types::DbParam{folder.cover_asset_id.value()}
                       : Core::Database::Types::DbParam{std::monostate{}});

  params.push_back(static_cast<int64_t>(folder.sort_order));
  params.push_back(folder.is_hidden);
  params.push_back(folder.id);

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to update folder: " + result.error());
  }

  return {};
}

auto delete_folder(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string> {
  // 暂时实现硬删除，实际项目中可能需要考虑级联删除等问题
  std::string sql = "DELETE FROM folders WHERE id = ?";
  std::vector<Core::Database::Types::DbParam> params = {id};

  auto result = Core::Database::execute(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to delete folder: " + result.error());
  }

  return {};
}

auto list_all_folders(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::Folder>, std::string> {
  std::string sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            ORDER BY path
        )";

  auto result = Core::Database::query<Types::Folder>(*app_state.database, sql);
  if (!result) {
    return std::unexpected("Failed to list all folders: " + result.error());
  }

  return result.value();
}

auto get_child_folders(Core::State::AppState& app_state, std::optional<std::int64_t> parent_id)
    -> std::expected<std::vector<Types::Folder>, std::string> {
  std::string sql;
  std::vector<Core::Database::Types::DbParam> params;

  if (parent_id.has_value()) {
    sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE parent_id = ?
            ORDER BY sort_order, name
        )";
    params.push_back(parent_id.value());
  } else {
    // 获取根文件夹（parent_id 为 NULL）
    sql = R"(
            SELECT id, path, parent_id, name, display_name, 
                   cover_asset_id, sort_order, is_hidden,
                   created_at, updated_at
            FROM folders
            WHERE parent_id IS NULL
            ORDER BY sort_order, name
        )";
  }

  auto result = Core::Database::query<Types::Folder>(*app_state.database, sql, params);
  if (!result) {
    return std::unexpected("Failed to get child folders: " + result.error());
  }

  return result.value();
}

auto get_or_create_folder_for_path(Core::State::AppState& app_state, const std::string& path)
    -> std::expected<std::int64_t, std::string> {
  // 首先尝试查找现有文件夹id
  auto existing_result = get_folder_by_path(app_state, path);
  if (!existing_result) {
    return std::unexpected("Failed to query existing folder: " + existing_result.error());
  }

  if (existing_result->has_value()) {
    return existing_result->value().id;
  }

  // 文件夹不存在，需要创建
  std::filesystem::path fs_path(path);
  std::string folder_name = fs_path.filename().string();

  Types::Folder new_folder{.path = path, .name = folder_name};

  auto create_result = create_folder(app_state, new_folder);
  if (!create_result) {
    return std::unexpected("Failed to create folder record: " + create_result.error());
  }

  Logger().debug("Created folder record for path '{}' with ID {}", path, create_result.value());
  return create_result.value();
}

}  // namespace Features::Gallery::Folder::Repository
