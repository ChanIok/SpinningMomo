#pragma once

#include "vendor/std.hpp"

#include "core/i18n/embedded/en_us.hpp"
#include "core/i18n/embedded/zh_cn.hpp"
#include "core/i18n/types.hpp"
#include "core/state/app_state.hpp"

namespace core::i18n {

auto initialize(core::AppState& state, Language default_lang = Language::EnUS)
    -> std::expected<void, std::string>;

auto load_language(core::AppState& state, Language lang) -> std::expected<void, std::string>;

// 使用 locale 字符串加载语言（例如 "zh-CN" / "en-US"）
auto load_language_by_locale(core::AppState& state, std::string_view locale)
    -> std::expected<void, std::string>;

auto get_current_language(const core::AppState& state) -> Language;

auto is_initialized(const core::AppState& state) -> bool;

}  // namespace core::i18n
