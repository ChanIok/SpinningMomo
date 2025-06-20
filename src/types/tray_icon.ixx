module;

export module Types.TrayIcon;

import Vendor.ShellApi;

namespace Types::TrayIcon {

export struct Data {
  Vendor::ShellApi::NOTIFYICONDATAW nid{};
  bool is_created = false;
};

}  // namespace Types::TrayIcon