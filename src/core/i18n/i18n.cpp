module;

#include <rfl.hpp>
#include <rfl/json.hpp>

module Core.I18n;

import std;
import Core.State;
import Core.I18n.Types;
import Core.I18n.State;
import Utils.Logger;

// 导入生成的嵌入模块
import Core.I18n.Embedded.ZhCN;
import Core.I18n.Embedded.EnUS;

namespace Core::I18n {

auto load_embedded_language_data(Types::Language lang)
    -> std::expected<std::string_view, std::string> {
  switch (lang) {
    case Types::Language::ZhCN:
      if (EmbeddedLocales::zh_cn_json.empty()) {
        return std::unexpected("Chinese language data is empty");
      }
      return EmbeddedLocales::zh_cn_json;

    case Types::Language::EnUS:
      if (EmbeddedLocales::en_us_json.empty()) {
        return std::unexpected("English language data is empty");
      }
      return EmbeddedLocales::en_us_json;

    default:
      return std::unexpected("Unsupported language");
  }
}

auto initialize(Core::State::AppState& app_state, Types::Language default_lang)
    -> std::expected<void, std::string> {
  try {
    // 重置状态为默认值
    app_state.i18n = State::I18nState{};

    // 加载默认语言到texts字段
    auto load_result = load_language(app_state, default_lang);
    if (!load_result) {
      return std::unexpected("Failed to load default language: " + load_result.error());
    }

    app_state.i18n.is_initialized = true;

    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Exception during I18n initialization: " + std::string(e.what()));
  }
}

auto load_language(Core::State::AppState& app_state, Types::Language lang)
    -> std::expected<void, std::string> {
  try {
    // 获取嵌入的语言数据
    auto data_result = load_embedded_language_data(lang);
    if (!data_result) {
      return std::unexpected(data_result.error());
    }

    // 解析JSON
    auto config_result = rfl::json::read<Types::TextData>(data_result.value());
    if (!config_result) {
      return std::unexpected("Failed to parse text data: " + config_result.error().what());
    }

    // 直接更新texts字段
    app_state.i18n.current_language = lang;
    app_state.i18n.texts = std::move(config_result.value());

    return {};
  } catch (const std::exception& e) {
    return std::unexpected("Exception during language loading: " + std::string(e.what()));
  }
}

auto get_current_language(const Core::State::AppState& app_state) -> Types::Language {
  return app_state.i18n.current_language;
}

auto is_initialized(const Core::State::AppState& app_state) -> bool {
  return app_state.i18n.is_initialized;
}

}  // namespace Core::I18n