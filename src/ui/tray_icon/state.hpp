#pragma once

#include "vendor/std.hpp"

#include "vendor/windows/shellapi.hpp"

namespace ui::tray_icon {

struct TrayIconState {
  NOTIFYICONDATAW nid{};
  bool is_created = false;
};

}  // namespace ui::tray_icon
