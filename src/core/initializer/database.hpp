#pragma once

#include "core/database/state.hpp"
#include "core/state/app_state.hpp"

namespace Core::Initializer::Database {

// 初始化数据库
auto initialize_database(Core::State::AppState& state) -> std::expected<void, std::string>;

}  // namespace Core::Initializer::Database