#pragma once

#include "vendor/std.hpp"

#include "core/i18n/types.hpp"

namespace core::i18n {

struct I18nState {
  Language current_language = Language::EnUS;
  TextData texts;
  bool is_initialized = false;
};

}  // namespace core::i18n
