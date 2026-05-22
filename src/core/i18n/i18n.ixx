module;

export module Core.I18n;

import std;
import Core.State;
import Core.I18n.Types;

// 导入生成的嵌入模块
import Core.I18n.Embedded.ZhCN;
import Core.I18n.Embedded.EnUS;

namespace Core::I18n {

export auto initialize(Core::State::AppState& state,
                       Types::Language default_lang = Types::Language::EnUS)
    -> std::expected<void, std::string>;

export auto load_language(Core::State::AppState& state, Types::Language lang)
    -> std::expected<void, std::string>;

// 使用 locale 字符串加载语言（例如 "zh-CN" / "en-US"）
export auto load_language_by_locale(Core::State::AppState& state, std::string_view locale)
    -> std::expected<void, std::string>;

export auto get_current_language(const Core::State::AppState& state) -> Types::Language;

export auto is_initialized(const Core::State::AppState& state) -> bool;

}  // namespace Core::I18n
