#include "features/gallery/ignore/matcher.hpp"

#include "vendor/std.hpp"

#include "utils/logger/logger.hpp"

namespace features::gallery::ignore::matcher {

namespace {

auto append_regex_literal(std::string& regex_pattern, char value) -> void {
  constexpr std::string_view regex_special_characters = R"(\.^$|()[]{}*+?)";
  if (regex_special_characters.contains(value)) {
    regex_pattern.push_back('\\');
  }
  regex_pattern.push_back(value);
}

auto append_character_class(const std::string& pattern, std::size_t& index,
                            std::string& regex_pattern) -> bool {
  auto end = pattern.find(']', index + 1);
  if (end == std::string::npos || end == index + 1) {
    return false;
  }

  regex_pattern.push_back('[');
  auto content_index = index + 1;
  if (pattern[content_index] == '!' || pattern[content_index] == '^') {
    regex_pattern.push_back('^');
    ++content_index;
  }
  if (content_index == end) {
    return false;
  }

  for (; content_index < end; ++content_index) {
    auto value = pattern[content_index];
    if (value == '\\' || value == '[') {
      regex_pattern.push_back('\\');
    }
    regex_pattern.push_back(value);
  }
  regex_pattern.push_back(']');
  index = end + 1;
  return true;
}

auto make_glob_regex(const std::string& pattern) -> std::optional<std::string> {
  std::string regex_pattern = "^";
  regex_pattern.reserve(pattern.size() * 2 + 2);

  for (std::size_t index = 0; index < pattern.size();) {
    const auto value = pattern[index];

    if (value == '*') {
      auto run_end = index;
      while (run_end < pattern.size() && pattern[run_end] == '*') {
        ++run_end;
      }

      const auto is_complete_segment = (index == 0 || pattern[index - 1] == '/') &&
                                       (run_end == pattern.size() || pattern[run_end] == '/');
      if (run_end - index >= 2 && is_complete_segment) {
        if (run_end < pattern.size()) {
          // **/ 匹配零个或多个完整路径段。
          regex_pattern += "(?:[^/]+/)*";
          index = run_end + 1;
        } else {
          regex_pattern += ".*";
          index = run_end;
        }
      } else {
        // 非完整路径段中的连续星号与单个 * 语义一致。
        regex_pattern += "[^/]*";
        index = run_end;
      }
      continue;
    }

    if (value == '?') {
      regex_pattern += "[^/]";
      ++index;
      continue;
    }

    if (value == '[') {
      if (!append_character_class(pattern, index, regex_pattern)) {
        return std::nullopt;
      }
      continue;
    }

    append_regex_literal(regex_pattern, value);
    ++index;
  }

  regex_pattern.push_back('$');
  return regex_pattern;
}

}  // namespace

auto match_glob_pattern(const std::string& pattern, const std::string& path) -> bool {
  auto regex_pattern = make_glob_regex(pattern);
  if (!regex_pattern) {
    Logger().warn("Invalid glob pattern '{}'", pattern);
    return false;
  }
  try {
    std::regex glob_regex(*regex_pattern, std::regex_constants::icase);
    return std::regex_match(path, glob_regex);
  } catch (const std::regex_error& e) {
    Logger().warn("Invalid glob pattern '{}': {}", pattern, e.what());
    return false;
  }
}

// ============= 正则表达式模式匹配 =============

auto match_regex_pattern(const std::string& pattern, const std::string& path) -> bool {
  try {
    std::regex regex_pattern(pattern, std::regex_constants::icase);
    return std::regex_search(path, regex_pattern);
  } catch (const std::regex_error& e) {
    Logger().warn("Invalid regex pattern '{}': {}", pattern, e.what());
    return false;
  }
}

}  // namespace features::gallery::ignore::matcher
