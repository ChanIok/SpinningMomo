#pragma once

#include "core/i18n/types.hpp"
#include "core/state/app_state.hpp"

// 导入生成的嵌入模块
#include "core/i18n/embedded/en_us.hpp"
#include "core/i18n/embedded/zh_cn.hpp"

namespace Core::I18n {

auto initialize(Core::State::AppState& state, Types::Language default_lang = Types::Language::EnUS)
    -> std::expected<void, std::string>;

auto load_language(Core::State::AppState& state, Types::Language lang)
    -> std::expected<void, std::string>;

// 使用 locale 字符串加载语言（例如 "zh-CN" / "en-US"）
auto load_language_by_locale(Core::State::AppState& state, std::string_view locale)
    -> std::expected<void, std::string>;

auto get_current_language(const Core::State::AppState& state) -> Types::Language;

auto is_initialized(const Core::State::AppState& state) -> bool;

}  // namespace Core::I18n
