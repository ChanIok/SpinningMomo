#pragma once

#include "vendor/std.hpp"

#include "core/database/state.hpp"
#include "core/state/app_state.hpp"

namespace core::initializer::database {

// 初始化数据库
auto initialize_database(core::AppState& state) -> std::expected<void, std::string>;

}  // namespace core::initializer::database
