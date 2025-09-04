module;

export module Core.Initializer.Database;

import std;
import Core.State;
import Core.Database.State;

namespace Core::Initializer::Database {

// 初始化数据库
export auto initialize_database(Core::State::AppState& state) -> std::expected<void, std::string>;

}  // namespace Core::Initializer::Database