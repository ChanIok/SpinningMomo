module;

export module Vendor.BuildConfig;

namespace Vendor::BuildConfig {

export constexpr bool is_debug_build() noexcept {
#ifdef NDEBUG
  return false;
#else
  return true;
#endif
}

}  // namespace Vendor::BuildConfig