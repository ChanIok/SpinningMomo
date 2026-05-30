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

// Playground 用 system.methodSignature 的 params_schema；关闭可省编译期 to_schema。
// 本地开发 Playground 时改为 true 后全量重编。
export constexpr bool rpc_json_schema_enabled() noexcept { return false; }

}  // namespace Vendor::BuildConfig