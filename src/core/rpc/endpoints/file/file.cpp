module;

#include <asio.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.RPC.Endpoints.File;

import std;
import Core.State;
import Core.RPC.Engine;
import Core.RPC.State;
import Core.RPC.Types;
import Utils.File;

namespace Core::RPC::Endpoints::File {

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

auto handle_read_file([[maybe_unused]] Core::State::AppState& app_state,
                      const ReadFileParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Utils::File::FileReadResult>> {
  auto result = co_await Utils::File::read_file(params.path);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to read file: " + result.error()});
  }

  co_return result.value();
}

auto handle_write_file([[maybe_unused]] Core::State::AppState& app_state,
                       const WriteFileParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Utils::File::FileWriteResult>> {
  auto result = co_await Utils::File::write_file(params.path, params.content, params.is_binary,
                                                 params.overwrite);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to write file: " + result.error()});
  }

  co_return result.value();
}

auto handle_list_directory([[maybe_unused]] Core::State::AppState& app_state,
                           const ListDirectoryParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Utils::File::DirectoryListResult>> {
  auto result = co_await Utils::File::list_directory(params.path, params.extensions);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to list directory: " + result.error()});
  }

  co_return result.value();
}

auto handle_get_file_info([[maybe_unused]] Core::State::AppState& app_state,
                          const GetFileInfoParams& params)
    -> asio::awaitable<Core::RPC::RpcResult<Utils::File::FileInfoResult>> {
  auto result = co_await Utils::File::get_file_info(params.path);
  if (!result) {
    co_return std::unexpected(
        Core::RPC::RpcError{.code = static_cast<int>(Core::RPC::ErrorCode::ServerError),
                            .message = "Failed to get file info: " + result.error()});
  }

  co_return result.value();
}

auto register_all(Core::State::AppState& app_state) -> void {
  register_method<ReadFileParams, Utils::File::FileReadResult>(
      app_state, app_state.rpc->registry, "file.read", handle_read_file,
      "Read file content with automatic text/binary detection and encoding");

  register_method<WriteFileParams, Utils::File::FileWriteResult>(
      app_state, app_state.rpc->registry, "file.write", handle_write_file,
      "Write content to file with text/binary support and optional overwrite protection");

  register_method<ListDirectoryParams, Utils::File::DirectoryListResult>(
      app_state, app_state.rpc->registry, "file.listDirectory", handle_list_directory,
      "List directory contents with optional file extension filtering");

  register_method<GetFileInfoParams, Utils::File::FileInfoResult>(
      app_state, app_state.rpc->registry, "file.getInfo", handle_get_file_info,
      "Get detailed information about a file or directory");
}

}  // namespace Core::RPC::Endpoints::File