#include "core/rpc/endpoints/file/file.hpp"

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "core/rpc/rpc.hpp"
#include "core/rpc/state.hpp"
#include "core/rpc/types.hpp"
#include "core/state/app_state.hpp"
#include "utils/file/file.hpp"
#include "utils/path/path.hpp"
#include "utils/system/system.hpp"

namespace core::rpc::endpoints::file {

struct ReadFileParams {
  std::string path;
};

struct WriteFileParams {
  std::string path;
  std::string content;
  bool is_binary{false};
  bool overwrite{true};
};

struct ListDirectoryParams {
  std::string path;
  std::vector<std::string> extensions{};
};

struct GetFileInfoParams {
  std::string path;
};

struct DeletePathParams {
  std::string path;
  bool recursive{false};
};

struct MovePathParams {
  std::string source_path;
  std::string destination_path;
  bool overwrite{false};
};

struct CopyPathParams {
  std::string source_path;
  std::string destination_path;
  bool recursive{false};
  bool overwrite{false};
};

struct OpenAppDataDirectoryResult {
  bool success;
  std::string message;
};

struct OpenAppDataDirectoryParams {};

struct OpenLogDirectoryResult {
  bool success;
  std::string message;
};

struct OpenLogDirectoryParams {};

auto handle_read_file([[maybe_unused]] core::AppState& app_state, const ReadFileParams& params)
    -> RpcAwaitable<utils::file::EncodedFileReadResult> {
  auto result = co_await utils::file::read_file_and_encode(params.path);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to read file: " + result.error()});
  }

  co_return result.value();
}

auto handle_write_file([[maybe_unused]] core::AppState& app_state, const WriteFileParams& params)
    -> RpcAwaitable<utils::file::FileWriteResult> {
  auto result = co_await utils::file::write_file(params.path, params.content, params.is_binary,
                                                 params.overwrite);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to write file: " + result.error()});
  }

  co_return result.value();
}

auto handle_list_directory([[maybe_unused]] core::AppState& app_state,
                           const ListDirectoryParams& params)
    -> RpcAwaitable<utils::file::DirectoryListResult> {
  auto result = co_await utils::file::list_directory(params.path, params.extensions);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to list directory: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_file_info([[maybe_unused]] core::AppState& app_state,
                          const GetFileInfoParams& params)
    -> RpcAwaitable<utils::file::FileInfoResult> {
  auto result = co_await utils::file::get_file_info(params.path);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to get file info: " + result.error()});
  }

  co_return result.value();
}

auto handle_delete_path([[maybe_unused]] core::AppState& app_state, const DeletePathParams& params)
    -> RpcAwaitable<utils::file::DeleteResult> {
  auto result = co_await utils::file::delete_path(params.path, params.recursive);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to delete path: " + result.error()});
  }

  co_return result.value();
}

auto handle_move_path([[maybe_unused]] core::AppState& app_state, const MovePathParams& params)
    -> RpcAwaitable<utils::file::MoveResult> {
  auto result = co_await utils::file::move_path(params.source_path, params.destination_path,
                                                params.overwrite);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to move path: " + result.error()});
  }

  co_return result.value();
}

auto handle_copy_path([[maybe_unused]] core::AppState& app_state, const CopyPathParams& params)
    -> RpcAwaitable<utils::file::CopyResult> {
  auto result = co_await utils::file::copy_path(params.source_path, params.destination_path,
                                                params.recursive, params.overwrite);
  if (!result) {
    co_return std::unexpected(RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                                       .message = "Failed to copy path: " + result.error()});
  }

  co_return result.value();
}

auto handle_open_app_data_directory([[maybe_unused]] core::AppState& app_state,
                                    [[maybe_unused]] const OpenAppDataDirectoryParams& params)
    -> RpcAwaitable<OpenAppDataDirectoryResult> {
  auto app_data_directory = utils::path::GetAppDataDirectory();
  if (!app_data_directory) {
    co_return std::unexpected(
        RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                 .message = "Failed to get app data directory: " + app_data_directory.error()});
  }

  auto open_result = utils::system::open_directory(app_data_directory.value());
  if (!open_result) {
    co_return std::unexpected(
        RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                 .message = "Failed to open app data directory: " + open_result.error()});
  }

  co_return OpenAppDataDirectoryResult{
      .success = true,
      .message = "App data directory opened successfully.",
  };
}

auto handle_open_log_directory([[maybe_unused]] core::AppState& app_state,
                               [[maybe_unused]] const OpenLogDirectoryParams& params)
    -> RpcAwaitable<OpenLogDirectoryResult> {
  auto log_directory = utils::path::GetAppDataSubdirectory("logs");
  if (!log_directory) {
    co_return std::unexpected(
        RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                 .message = "Failed to get log directory: " + log_directory.error()});
  }

  auto open_result = utils::system::open_directory(log_directory.value());
  if (!open_result) {
    co_return std::unexpected(
        RpcError{.code = static_cast<int>(ErrorCode::ServerError),
                 .message = "Failed to open log directory: " + open_result.error()});
  }

  co_return OpenLogDirectoryResult{
      .success = true,
      .message = "Log directory opened successfully.",
  };
}

auto register_all(core::AppState& app_state) -> void {
  register_method<ReadFileParams, utils::file::EncodedFileReadResult>(
      app_state, app_state.rpc->registry, "file.read", handle_read_file,
      "Read file content with automatic text/binary detection and encoding");

  register_method<WriteFileParams, utils::file::FileWriteResult>(
      app_state, app_state.rpc->registry, "file.write", handle_write_file,
      "Write content to file with text/binary support and optional overwrite protection");

  register_method<ListDirectoryParams, utils::file::DirectoryListResult>(
      app_state, app_state.rpc->registry, "file.listDirectory", handle_list_directory,
      "List directory contents with optional file extension filtering");

  register_method<GetFileInfoParams, utils::file::FileInfoResult>(
      app_state, app_state.rpc->registry, "file.getInfo", handle_get_file_info,
      "Get detailed information about a file or directory");

  register_method<DeletePathParams, utils::file::DeleteResult>(
      app_state, app_state.rpc->registry, "file.delete", handle_delete_path,
      "Delete file or directory with optional recursive deletion");

  register_method<MovePathParams, utils::file::MoveResult>(
      app_state, app_state.rpc->registry, "file.move", handle_move_path,
      "Move or rename file/directory with optional overwrite protection");

  register_method<CopyPathParams, utils::file::CopyResult>(
      app_state, app_state.rpc->registry, "file.copy", handle_copy_path,
      "Copy file or directory with optional recursive copy and overwrite protection");

  register_method<OpenAppDataDirectoryParams, OpenAppDataDirectoryResult>(
      app_state, app_state.rpc->registry, "file.openAppDataDirectory",
      handle_open_app_data_directory, "Open app data directory in file explorer");

  register_method<OpenLogDirectoryParams, OpenLogDirectoryResult>(
      app_state, app_state.rpc->registry, "file.openLogDirectory", handle_open_log_directory,
      "Open log directory in file explorer");
}

}  // namespace core::rpc::endpoints::file
