module;

export module Core.I18n.State;

import std;
import Core.I18n.Types;

namespace Core::I18n::State {

export struct I18nState {
  Types::Language current_language = Types::Language::EnUS;
  Types::TextData texts;
  bool is_initialized = false;
};

}  // namespace Core::I18n::State
