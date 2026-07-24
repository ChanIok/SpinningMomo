#pragma once

#include "vendor/shellapi.hpp"

namespace UI::TrayIcon::State {

struct TrayIconState {
  Vendor::ShellApi::NOTIFYICONDATAW nid{};
  bool is_created = false;
};

}  // namespace UI::TrayIcon::State