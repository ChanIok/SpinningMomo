#include "core/initializer/database.hpp"

#include "vendor/std.hpp"

#include "core/database/database.hpp"
#include "core/database/state.hpp"
#include "core/state/app_state.hpp"
#include "utils/logger/logger.hpp"
#include "utils/path/path.hpp"

namespace core::initializer::database {

auto initialize_database(core::AppState& state) -> std::expected<void, std::string> {
  try {
    // 初始化数据库连接
    auto path_result = utils::path::GetAppDataFilePath("database.db");
    if (!path_result) {
      return std::unexpected("Failed to get database path: " + path_result.error());
    }
    const auto db_path = path_result.value();
    if (auto result = core::database::initialize(state, db_path); !result) {
      Logger().error("Failed to initialize database: {}", result.error());
      return std::unexpected("Failed to initialize database: " + result.error());
    }

    Logger().info("Database initialized successfully at {}", db_path.string());
    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Exception during database initialization: " + std::string(e.what()));
  }
}

}  // namespace core::initializer::database
