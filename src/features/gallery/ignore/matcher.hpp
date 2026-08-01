#pragma once

#include "vendor/std.hpp"

namespace features::gallery::ignore::matcher {

// 对根目录相对路径执行整路径 Glob 匹配，* 不跨目录，** 可跨目录。
auto match_glob_pattern(const std::string& pattern, const std::string& path) -> bool;

// 正则表达式模式匹配
auto match_regex_pattern(const std::string& pattern, const std::string& path) -> bool;

}  // namespace features::gallery::ignore::matcher
