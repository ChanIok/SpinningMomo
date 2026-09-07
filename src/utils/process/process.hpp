#pragma once

#include "vendor/std.hpp"

#include "vendor/wil.hpp"
#include "vendor/windows.hpp"

namespace utils::process {

// 拥有一个长期运行的 Windows 子进程及其可选的 stderr 诊断文件。
struct ChildProcess {
  wil::unique_handle process;
  std::filesystem::path stderr_file;
};

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

// 长期子进程的启动选项。stdout 保持静默，stderr 可落到文件供启动失败诊断。
struct SpawnOptions {
  std::filesystem::path working_directory;
  std::filesystem::path stderr_file;
};

// 直接启动可执行文件，不经过 cmd.exe 或 PowerShell。
// arguments 的每一项都是独立参数，函数负责构造 Windows 命令行并捕获输出。
auto run(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
         const RunOptions& options = {}) -> std::expected<CommandResult, std::string>;

// 启动长期子进程，不等待退出；适用于 ADB Android 服务。
auto spawn(const std::filesystem::path& executable, const std::vector<std::wstring>& arguments,
           const SpawnOptions& options = {}) -> std::expected<ChildProcess, std::string>;

// 查询长期子进程当前退出码；STILL_ACTIVE 表示仍在运行。
auto get_exit_code(const ChildProcess& child) -> std::expected<std::uint32_t, std::string>;

// 读取启动过程中由子进程写入的 stderr 诊断文本。
auto read_stderr(const ChildProcess& child) -> std::expected<std::string, std::string>;

// 等待子进程结束；超时只返回错误，不会自动终止进程。
auto wait(ChildProcess& child, std::chrono::milliseconds timeout)
    -> std::expected<std::uint32_t, std::string>;

// 终止并关闭子进程；目标已经退出时视为成功。
auto terminate(ChildProcess& child) -> std::expected<void, std::string>;

}  // namespace utils::process
