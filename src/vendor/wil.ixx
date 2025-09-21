module;

#include <wil/resource.h>

export module Vendor.WIL;

namespace Vendor::WIL {

export using ::wil::make_unique_hlocal_nothrow;
export using ::_wdupenv_s;
export using ::free;

} // namespace Vendor::WIL