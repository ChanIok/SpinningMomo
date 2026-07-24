#pragma once

namespace Core::Database::Types {
// 代表数据库中的一个值，可以是NULL、整数、浮点数、字符串或二进制数据
using DbValue =
    std::variant<std::monostate, std::int64_t, double, std::string, std::vector<std::uint8_t>>;

// 用于参数化查询的参数类型
using DbParam = DbValue;

}  // namespace Core::Database::Types