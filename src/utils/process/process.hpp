#pragma once

#include "vendor/std.hpp"

namespace utils::process {

// 子进程执行结果。stdout/stderr 均按原始字节保存，因此也可用于截图等二进制输出。
struct CommandResult {
  std::uint32_t exit_code = 0;
  std::string stdout_data;
  std::string stderr_data;
  bool timed_out = false;
};

struct RunOptions {
  std::chrono::milliseconds timeout = std::chrono::seconds(30);
  std::filesystem::path working_directory;
};

// 直接启动可执行文件，不经过 cmd.exe 或 PowerShell。
// arguments 的每一项都是独立参数，函数负责构造 Windows 命令行并捕获输出。
auto run(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
         const RunOptions& options = {}) -> std::expected<CommandResult, std::string>;

}  // namespace utils::process
