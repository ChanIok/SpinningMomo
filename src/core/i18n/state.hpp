#pragma once

#include "core/i18n/types.hpp"

namespace Core::I18n::State {

struct I18nState {
  Types::Language current_language = Types::Language::EnUS;
  Types::TextData texts;
  bool is_initialized = false;
};

}  // namespace Core::I18n::State
