module;

#include <SQLiteCpp/SQLiteCpp.h>

#include <rfl.hpp>

export module Core.Database.DataMapper;

import std;
import Core.Database.Types;

namespace Core::Database::DataMapper {

export enum class MappingErrorType {
  field_not_found,
  type_conversion_failed,
  validation_failed,
  null_value_for_required_field
};

export struct MappingError {
  MappingErrorType type;
  std::string field_name;
  std::string details;

  auto to_string() const -> std::string {
    std::string type_str;
    switch (type) {
      case MappingErrorType::field_not_found:
        type_str = "Field not found";
        break;
      case MappingErrorType::type_conversion_failed:
        type_str = "Type conversion failed";
        break;
      case MappingErrorType::validation_failed:
        type_str = "Validation failed";
        break;
      case MappingErrorType::null_value_for_required_field:
        type_str = "Null value for required field";
        break;
    }
    return type_str + " for field '" + field_name + "': " + details;
  }
};

export using MappingResult = std::vector<MappingError>;

// 类型转换器
export template <typename T>
struct SqliteTypeConverter;

// 基础类型特化
template <>
struct SqliteTypeConverter<int> {
  static auto from_column(const SQLite::Column& col) -> std::expected<int, std::string> {
    if (col.isNull()) {
      return std::unexpected("Column is NULL");
    }
    try {
      return col.getInt();
    } catch (const SQLite::Exception& e) {
      return std::unexpected("SQLite error: " + std::string(e.what()));
    }
  }
};

template <>
struct SqliteTypeConverter<int64_t> {
  static auto from_column(const SQLite::Column& col) -> std::expected<int64_t, std::string> {
    if (col.isNull()) {
      return std::unexpected("Column is NULL");
    }
    try {
      return col.getInt64();
    } catch (const SQLite::Exception& e) {
      return std::unexpected("SQLite error: " + std::string(e.what()));
    }
  }
};

template <>
struct SqliteTypeConverter<double> {
  static auto from_column(const SQLite::Column& col) -> std::expected<double, std::string> {
    if (col.isNull()) {
      return std::unexpected("Column is NULL");
    }
    try {
      return col.getDouble();
    } catch (const SQLite::Exception& e) {
      return std::unexpected("SQLite error: " + std::string(e.what()));
    }
  }
};

template <>
struct SqliteTypeConverter<std::string> {
  static auto from_column(const SQLite::Column& col) -> std::expected<std::string, std::string> {
    if (col.isNull()) {
      return std::unexpected("Column is NULL");
    }
    try {
      return col.getString();
    } catch (const SQLite::Exception& e) {
      return std::unexpected("SQLite error: " + std::string(e.what()));
    }
  }
};

// std::optional 特化 - 可以处理NULL值
template <typename T>
struct SqliteTypeConverter<std::optional<T>> {
  static auto from_column(const SQLite::Column& col)
      -> std::expected<std::optional<T>, std::string> {
    if (col.isNull()) {
      return std::optional<T>{};
    }

    auto result = SqliteTypeConverter<T>::from_column(col);
    if (!result) {
      return std::unexpected(result.error());
    }
    return std::optional<T>{std::move(result.value())};
  }
};

// 提取字段值
export template <typename FieldType>
auto extract_field_value(SQLite::Statement& stmt, const std::string& field_name)
    -> std::expected<FieldType, MappingError> {
  try {
    // 尝试获取列
    SQLite::Column col = stmt.getColumn(field_name.c_str());

    // 使用类型转换器
    auto result = SqliteTypeConverter<FieldType>::from_column(col);
    if (!result) {
      return std::unexpected(MappingError{.type = MappingErrorType::type_conversion_failed,
                                          .field_name = field_name,
                                          .details = result.error()});
    }

    return result.value();

  } catch (const SQLite::Exception& e) {
    // 字段不存在或其他SQLite错误
    return std::unexpected(MappingError{.type = MappingErrorType::field_not_found,
                                        .field_name = field_name,
                                        .details = std::string(e.what())});
  }
}

// 主要接口 - 对象构建器
export template <typename T>
auto from_statement(SQLite::Statement& query) -> std::expected<T, std::string> {
  T object{};
  std::vector<MappingError> errors;

  // 使用 rfl 遍历结构体的所有字段
  rfl::to_view(object).apply([&](auto field) {
    const std::string field_name = std::string(field.name());
    using FieldValueType = std::remove_cvref_t<decltype(*field.value())>;

    // 直接提取字段值，使用字段名
    auto result = extract_field_value<FieldValueType>(query, field_name);
    if (result) {
      *field.value() = std::move(result.value());
    } else {
      errors.push_back(result.error());
    }
  });

  // 如果有错误，合并错误信息
  if (!errors.empty()) {
    std::string error_message = "Found " + std::to_string(errors.size()) + " errors:\n";
    for (size_t i = 0; i < errors.size(); ++i) {
      error_message += std::to_string(i + 1) + ") " + errors[i].to_string() + "\n";
    }
    return std::unexpected(error_message);
  }

  return object;
}
}  // namespace Core::Database::DataMapper