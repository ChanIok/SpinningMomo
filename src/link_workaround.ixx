module;

// 该文件是针对使用 C++23 模块时 MSVC 链接器问题（error
// LNK2001）的解决方法，特别是在使用静态运行时链接 (/MT) 时。
// 在此包含这些头文件并强制导出该类函数，确保链接器能够为其他模块找到必要的符号。

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

export module link_workaround;

// 时间格式化
export [[maybe_unused]] auto format_timestamp(
    const std::chrono::time_point<std::chrono::system_clock>& time_point) -> std::string {
  return std::format("{:%Y-%m-%d %H:%M:%S}", time_point);
}

// 文件流操作的链接器解决方案
export [[maybe_unused]] auto create_ofstream(const std::string& filepath) -> std::ofstream {
  return std::ofstream(filepath, std::ios::out | std::ios::trunc);
}

export [[maybe_unused]] auto write_to_stream(std::ofstream& stream, const std::string& message)
    -> void {
  if (stream.is_open()) {
    stream << message << std::endl;
    stream.flush();
  }
}

export [[maybe_unused]] auto close_stream(std::ofstream& stream) -> void {
  if (stream.is_open()) {
    stream.close();
  }
}

export [[maybe_unused]] auto is_stream_open(const std::ofstream& stream) -> bool {
  return stream.is_open();
}

// std::cout 链接器解决方案
export [[maybe_unused]] auto print_to_cout(const std::string& message) -> void {
  std::cout << message;
}

export [[maybe_unused]] auto println_to_cout(const std::string& message) -> void {
  std::cout << message << std::endl;
}

export [[maybe_unused]] auto flush_cout() -> void {
  std::cout.flush();
}

export [[maybe_unused]] auto print_newline() -> void {
  std::cout << std::endl;
}

export [[maybe_unused]] auto get_cout_reference() -> std::ostream& {
  return std::cout;
}