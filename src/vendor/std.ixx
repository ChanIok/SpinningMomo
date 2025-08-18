module;

#include <cmath>

export module Vendor.Std;

namespace Vendor::Std {

// 只导出有冲突的数学函数
export auto round(double x) -> double { return ::std::round(x); }

}  // namespace Vendor::Std 