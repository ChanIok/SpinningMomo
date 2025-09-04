module;

export module Core.Database.Types;

import std;

export namespace Core::Database::Types {
// 代表数据库中的一个值，可以是NULL、整数、浮点数、字符串或二进制数据
export using DbValue =
    std::variant<std::monostate, std::int64_t, double, std::string, std::vector<std::uint8_t>>;

// 用于参数化查询的参数类型
export using DbParam = DbValue;
}  // namespace Core::Database::Types