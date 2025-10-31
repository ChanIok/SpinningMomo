module;

export module Features.Gallery.Ignore.Repository;

import std;
import Core.State;
import Features.Gallery.Types;

namespace Features::Gallery::Ignore::Repository {

export auto create_ignore_rule(Core::State::AppState& app_state, const Types::IgnoreRule& rule)
    -> std::expected<std::int64_t, std::string>;

export auto get_ignore_rule_by_id(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<std::optional<Types::IgnoreRule>, std::string>;

export auto update_ignore_rule(Core::State::AppState& app_state, const Types::IgnoreRule& rule)
    -> std::expected<void, std::string>;

export auto delete_ignore_rule(Core::State::AppState& app_state, std::int64_t id)
    -> std::expected<void, std::string>;

export auto get_rules_by_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

export auto get_rules_by_directory_path(Core::State::AppState& app_state,
                                        const std::string& directory_path)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

export auto get_global_rules(Core::State::AppState& app_state)
    -> std::expected<std::vector<Types::IgnoreRule>, std::string>;

export auto batch_create_ignore_rules(Core::State::AppState& app_state, std::int64_t folder_id,
                                      const std::vector<Types::ScanIgnoreRule>& scan_rules)
    -> std::expected<std::vector<std::int64_t>, std::string>;

export auto batch_update_ignore_rules(Core::State::AppState& app_state,
                                      const std::vector<Types::IgnoreRule>& rules)
    -> std::expected<void, std::string>;

export auto delete_rules_by_folder_id(Core::State::AppState& app_state, std::int64_t folder_id)
    -> std::expected<int, std::string>;

export auto toggle_rule_enabled(Core::State::AppState& app_state, std::int64_t id, bool enabled)
    -> std::expected<void, std::string>;

export auto cleanup_orphaned_rules(Core::State::AppState& app_state)
    -> std::expected<int, std::string>;

export auto count_rules(Core::State::AppState& app_state,
                        std::optional<std::int64_t> folder_id = std::nullopt)
    -> std::expected<int, std::string>;

}  // namespace Features::Gallery::Ignore::Repository
