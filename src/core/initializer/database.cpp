module;

module Core.Initializer.Database;

import std;
import Core.State;
import Core.Database;
import Core.Database.State;
import Utils.Logger;
import Utils.Path;

namespace Core::Initializer::Database {

auto initialize_database(Core::State::AppState& state) -> std::expected<void, std::string> {
  try {
    // 初始化数据库连接
    auto path_result = Utils::Path::GetExecutableDirectory();
    if (!path_result) {
      return std::unexpected("Failed to get executable directory: " + path_result.error());
    }
    const auto db_path = path_result.value() / "database.db";
    if (auto result = Core::Database::initialize(*state.database, db_path); !result) {
      Logger().error("Failed to initialize database: {}", result.error());
      return std::unexpected("Failed to initialize database: " + result.error());
    }

    Logger().info("Database initialized successfully at {}", db_path.string());
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Exception during database initialization: " + std::string(e.what()));
  }
}

}  // namespace Core::Initializer::Database