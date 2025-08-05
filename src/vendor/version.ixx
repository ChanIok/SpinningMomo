module;

#include "version.hpp"

export module Vendor.Version;

import std;

export namespace Vendor::Version {

export auto get_app_version() -> std::string { return VERSION_STR; }

}  // namespace Vendor::Version