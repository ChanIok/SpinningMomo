module;

export module Vendor.Version;

import std;

namespace Vendor::Version {

export auto get_app_version() -> std::string { return "2.0.2.0"; }

}  // namespace Vendor::Version
