module;

export module Core.Database.State;

import std;

export namespace Core::Database::State {

struct DatabaseState {
  // 存储数据库文件路径
  std::filesystem::path db_path;
};

}  // namespace Core::Database::State