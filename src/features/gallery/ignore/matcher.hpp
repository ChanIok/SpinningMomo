#pragma once

#include "vendor/std.hpp"

namespace features::gallery::ignore::matcher {

// Glob 模式匹配
auto match_glob_pattern(const std::string& pattern, const std::string& path) -> bool;

// 正则表达式模式匹配
auto match_regex_pattern(const std::string& pattern, const std::string& path) -> bool;

}  // namespace features::gallery::ignore::matcher
