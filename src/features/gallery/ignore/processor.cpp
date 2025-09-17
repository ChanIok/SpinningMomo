module;

#include <regex>

module Features.Gallery.Ignore.Processor;

import std;
import Core.State;
import Features.Gallery.Types;
import Utils.Logger;

namespace Features::Gallery::Ignore::Processor {

// ============= 路径处理辅助函数 =============

auto normalize_path_for_matching(const std::filesystem::path& file_path,
                                 const std::filesystem::path& base_path) -> std::string {
  try {
    // 计算相对路径并转换为字符串
    auto relative = std::filesystem::relative(file_path, base_path);
    auto path_str = relative.string();

    // 统一使用正斜杠（Unix风格），便于模式匹配
    std::ranges::replace(path_str, '\\', '/');

    return path_str;
  } catch (const std::filesystem::filesystem_error&) {
    // 如果无法计算相对路径，返回文件名
    auto filename = file_path.filename().string();
    std::ranges::replace(filename, '\\', '/');
    return filename;
  }
}

// ============= Glob模式匹配实现 =============

auto match_glob_pattern(const std::string& pattern, const std::string& path) -> bool {
  // 简化的glob实现，支持基本的gitignore模式
  // TODO: 这是一个基础实现，后续可以扩展支持更复杂的glob语法

  try {
    // 转换glob模式为正则表达式
    std::string regex_pattern;
    regex_pattern.reserve(pattern.size() * 2);

    bool in_bracket = false;

    for (size_t i = 0; i < pattern.size(); ++i) {
      char c = pattern[i];

      switch (c) {
        case '*':
          if (i + 1 < pattern.size() && pattern[i + 1] == '*') {
            // ** 匹配任何目录层级
            regex_pattern += ".*";
            ++i;  // 跳过第二个*
            // 跳过后续的/
            if (i + 1 < pattern.size() && pattern[i + 1] == '/') {
              ++i;
            }
          } else {
            // * 匹配除/外的任意字符
            regex_pattern += "[^/]*";
          }
          break;
        case '?':
          regex_pattern += "[^/]";
          break;
        case '[':
          regex_pattern += "[";
          in_bracket = true;
          break;
        case ']':
          regex_pattern += "]";
          in_bracket = false;
          break;
        case '.':
        case '+':
        case '^':
        case '$':
        case '(':
        case ')':
        case '{':
        case '}':
        case '|':
          // 转义正则表达式特殊字符
          if (!in_bracket) {
            regex_pattern += "\\";
          }
          regex_pattern += c;
          break;
        default:
          regex_pattern += c;
          break;
      }
    }

    // 如果模式不以/开头，则匹配任何位置
    if (!pattern.starts_with("/")) {
      regex_pattern = "(^|.*/)" + regex_pattern;
    } else {
      // 移除开头的/
      if (regex_pattern.starts_with("/")) {
        regex_pattern = regex_pattern.substr(1);
      }
      regex_pattern = "^" + regex_pattern;
    }

    // 如果模式以/结尾，表示只匹配目录
    if (pattern.ends_with("/")) {
      regex_pattern += "$";
    } else {
      // 匹配文件或目录
      regex_pattern += "(/.*)?$";
    }

    std::regex glob_regex(regex_pattern, std::regex_constants::icase);
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

// ============= 忽略规则应用 =============

auto apply_ignore_rules(const std::filesystem::path& file_path,
                        const std::filesystem::path& base_path,
                        const std::vector<Types::IgnoreRule>& rules) -> bool {
  if (rules.empty()) {
    return false;  // 没有规则，不忽略
  }

  auto normalized_path = normalize_path_for_matching(file_path, base_path);
  bool should_ignore = false;

  // 按顺序应用规则，后面的规则会覆盖前面的结果
  for (const auto& rule : rules) {
    if (!rule.is_enabled) {
      continue;  // 跳过禁用的规则
    }

    bool matches = false;

    // 根据模式类型选择匹配方法
    if (rule.pattern_type == "glob") {
      matches = match_glob_pattern(rule.rule_pattern, normalized_path);
    } else if (rule.pattern_type == "regex") {
      matches = match_regex_pattern(rule.rule_pattern, normalized_path);
    } else {
      Logger().warn("Unknown pattern type '{}' for rule: {}", rule.pattern_type, rule.rule_pattern);
      continue;
    }

    if (matches) {
      // 根据规则类型设置忽略状态
      should_ignore = (rule.rule_type == "exclude");

      Logger().debug("File '{}' {} by rule '{}' ({})", normalized_path,
                     should_ignore ? "excluded" : "included", rule.rule_pattern, rule.pattern_type);
    }
  }

  return should_ignore;
}

// ============= 辅助函数 =============

// 检查规则是否匹配文件
auto check_rule_match(const Types::IgnoreRule& rule, const std::string& normalized_path) -> bool {
  if (!rule.is_enabled) {
    return false;
  }

  bool matches = false;
  if (rule.pattern_type == "glob") {
    matches = match_glob_pattern(rule.rule_pattern, normalized_path);
  } else if (rule.pattern_type == "regex") {
    matches = match_regex_pattern(rule.rule_pattern, normalized_path);
  } else {
    Logger().warn("Unknown pattern type '{}' for rule: {}", rule.pattern_type, rule.rule_pattern);
    return false;
  }

  return matches;
}

// 应用规则组并返回最终的忽略状态
auto apply_rules_with_logging(const std::vector<Types::IgnoreRule>& rules,
                              const std::string& normalized_path, const std::string& rule_type_name,
                              bool initial_state = false) -> bool {
  bool should_ignore = initial_state;

  for (const auto& rule : rules) {
    if (check_rule_match(rule, normalized_path)) {
      should_ignore = (rule.rule_type == "exclude");
      Logger().debug("File '{}' {} by {} rule '{}' ({})", normalized_path,
                     should_ignore ? "excluded" : "included", rule_type_name, rule.rule_pattern,
                     rule.pattern_type);
    }
  }

  return should_ignore;
}

// ============= 主要处理函数 =============

auto should_ignore_file(const std::filesystem::path& file_path,
                        const std::filesystem::path& base_path, const Types::IgnoreContext& context,
                        std::optional<std::int64_t> folder_id) -> bool {
  auto normalized_path = normalize_path_for_matching(file_path, base_path);

  // 第一阶段：应用全局规则
  bool should_ignore = apply_rules_with_logging(context.global_rules, normalized_path, "GLOBAL");

  // 第二阶段：如果有文件夹规则，在全局规则的基础上继续应用（可以覆盖）
  if (folder_id.has_value()) {
    auto it = context.folder_rules.find(folder_id.value());
    if (it != context.folder_rules.end()) {
      should_ignore =
          apply_rules_with_logging(it->second, normalized_path, "FOLDER", should_ignore);
    }
  }

  return should_ignore;
}

}  // namespace Features::Gallery::Ignore::Processor
