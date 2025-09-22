module;

#include <wil/result.h>

export module Vendor.WIL;

namespace Vendor::WIL {

// Modern error handling - replaces THROW_IF_FAILED macro
export auto throw_if_failed(HRESULT hr) -> void {
  if (FAILED(hr)) {
    throw wil::ResultException(hr);
  }
}

}  // namespace Vendor::WIL
