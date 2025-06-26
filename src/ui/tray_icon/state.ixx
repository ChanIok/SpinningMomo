module;

export module UI.TrayIcon.State;

import Vendor.ShellApi;

namespace UI::TrayIcon::State {

export struct Data {
  Vendor::ShellApi::NOTIFYICONDATAW nid{};
  bool is_created = false;
};

}  // namespace UI::TrayIcon::Sta