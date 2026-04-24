module;

module Features.Gallery.Ignore.Service;

import std;
import Core.State;
import Features.Gallery.Types;
import Features.Gallery.Folder.Repository;
import Features.Gallery.Ignore.Repository;
import Features.Gallery.Ignore.Matcher;
import Utils.Logger;

namespace Features::Gallery::Ignore::Service {

// ============= 路径处理辅助函数 =============

auto normalize_path_for_matching(const std::filesystem::path& file_path,
                                 const std::filesystem::path& base_path) -> std::string {
  try {
    // 计算相对路径，自动规范化，并使用统一的正斜杠格式
    auto relative = std::filesystem::relative(file_path, base_path);
    return relative.lexically_normal().generic_string();
  } catch (const std::filesystem::filesystem_error&) {
    // 如果无法计算相对路径，返回规范化的文件名
    return file_path.filename().lexically_normal().generic_string();
  }
}

// ============= 业务编排函数 =============

auto resolve_root_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::int64_t, std::string> {
  std::unordered_set<std::int64_t> visited_ids;

  std::optional<std::int64_t> current_id = folder_id;
  while (current_id.has_value()) {
    if (!visited_ids.insert(*current_id).second) {
      return std::unexpected("Detected folder parent cycle while loading ignore rules: " +
                             std::to_string(*current_id));
    }

    auto folder_result =
        Features::Gallery::Folder::Repository::get_folder_by_id(app_state, *current_id);
    if (!folder_result) {
      return std::unexpected("Failed to query folder while loading ignore rules: " +
                             folder_result.error());
    }
    if (!folder_result->has_value()) {
      return std::unexpected("Folder not found while loading ignore rules: " +
                             std::to_string(*current_id));
    }

    const auto& folder = folder_result->value();
    if (!folder.parent_id.has_value()) {
      return folder.id;
    }

    current_id = folder.parent_id;
  }

  return std::unexpected("Failed to resolve root folder while loading ignore rules: " +
                         std::to_string(folder_id));
}

auto load_ignore_rules(Core::State::AppState& app_state, std::optional<std::int64_t> folder_id)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string> {
  std::vector<Types::IgnoreRule> combined_rules;

  // 1. 先加载全局规则
  auto global_rules_result = Repository::get_global_rules(app_state);
  if (!global_rules_result) {
    return std::unexpected("Failed to load global ignore rules: " + global_rules_result.error());
  }
  combined_rules = std::move(global_rules_result.value());

  // 2. 对目录扫描只读取所属 root 文件夹的规则。
  if (folder_id.has_value()) {
    auto root_folder_id_result = resolve_root_folder_id(app_state, *folder_id);
    if (!root_folder_id_result) {
      return std::unexpected("Failed to resolve root folder ignore rules: " +
                             root_folder_id_result.error());
    }

    auto folder_rules_result =
        Repository::get_rules_by_folder_id(app_state, root_folder_id_result.value());
    if (!folder_rules_result) {
      Logger().warn("Failed to load root-folder ignore rules for folder_id {}: {}",
                    root_folder_id_result.value(), folder_rules_result.error());
    } else {
      auto& folder_rules = folder_rules_result.value();
      combined_rules.insert(combined_rules.end(), std::make_move_iterator(folder_rules.begin()),
                            std::make_move_iterator(folder_rules.end()));
    }
  }

  return combined_rules;
}

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
      matches = Matcher::match_glob_pattern(rule.rule_pattern, normalized_path);
    } else if (rule.pattern_type == "regex") {
      matches = Matcher::match_regex_pattern(rule.rule_pattern, normalized_path);
    } else {
      Logger().warn("Unknown pattern type '{}' for rule: {}", rule.pattern_type, rule.rule_pattern);
      continue;
    }

    if (matches) {
      // 根据规则类型设置忽略状态
      should_ignore = (rule.rule_type == "exclude");
    }
  }

  return should_ignore;
}

}  // namespace Features::Gallery::Ignore::Service
