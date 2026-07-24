#pragma once

#include "vendor/std.hpp"

#include "core/state/app_state.hpp"
#include "features/gallery/types.hpp"

namespace features::gallery::ignore::repository {

auto create_ignore_rule(core::AppState& app_state, const IgnoreRule& rule)
    -> std::expected<std::int64_t, std::string>;

auto get_ignore_rule_by_id(core::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<IgnoreRule>, std::string>;

auto update_ignore_rule(core::AppState& app_state, const IgnoreRule& rule)
    -> std::expected<void, std::string>;

auto delete_ignore_rule(core::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

auto get_rules_by_folder_id(core::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::vector<IgnoreRule>, std::string>;

auto get_rules_by_directory_path(core::AppState& app_state, const std::string& directory_path)
    -> std::expected<std::vector<IgnoreRule>, std::string>;

auto get_global_rules(core::AppState& app_state)
    -> std::expected<std::vector<IgnoreRule>, std::string>;

auto replace_rules_by_folder_id(core::AppState& app_state, std::int64_t folder_id,
                                const std::vector<ScanIgnoreRule>& scan_rules)
    -> std::expected<void, std::string>;

auto batch_update_ignore_rules(core::AppState& app_state, const std::vector<IgnoreRule>& rules)
    -> std::expected<void, std::string>;

auto delete_rules_by_folder_id(core::AppState& app_state, std::int64_t folder_id)
    -> std::expected<int, std::string>;

auto toggle_rule_enabled(core::AppState& app_state, std::int64_t id, bool enabled)
    -> std::expected<void, std::string>;

auto cleanup_orphaned_rules(core::AppState& app_state) -> std::expected<int, std::string>;

auto count_rules(core::AppState& app_state, std::optional<std::int64_t> folder_id = std::nullopt)
    -> std::expected<int, std::string>;

}  // namespace features::gallery::ignore::repository
