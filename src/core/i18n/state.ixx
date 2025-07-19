module;

export module Core.I18n.State;

import std;
import Core.I18n.Types;

export namespace Core::I18n::State {

struct I18nState {
  Types::Language current_language = Types::Language::EnUS;
  Types::TextData texts;
  bool is_initialized = false;

  I18nState() = default;
};

}  // namespace Core::I18n::State