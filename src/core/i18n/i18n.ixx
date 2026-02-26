module;

export module Core.I18n;

import std;
import Core.I18n.Types;
import Core.I18n.State;

// 导入生成的嵌入模块
import Core.I18n.Embedded.ZhCN;
import Core.I18n.Embedded.EnUS;

namespace Core::I18n {

export auto initialize(State::I18nState& i18n_state,
                       Types::Language default_lang = Types::Language::EnUS)
    -> std::expected<void, std::string>;

export auto load_language(State::I18nState& i18n_state, Types::Language lang)
    -> std::expected<void, std::string>;

// 使用 locale 字符串加载语言（例如 "zh-CN" / "en-US"）
export auto load_language_by_locale(State::I18nState& i18n_state, std::string_view locale)
    -> std::expected<void, std::string>;

export auto get_current_language(const State::I18nState& i18n_state) -> Types::Language;

export auto is_initialized(const State::I18nState& i18n_state) -> bool;

}  // namespace Core::I18n
