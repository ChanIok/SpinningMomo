#pragma once

#include "vendor/std.hpp"

namespace utils::powershell {

// 同步执行 PowerShell 脚本并返回退出码，供调用方确认脚本结果。
auto run_script_and_wait(const std::filesystem::path& script_path,
                         const std::vector<std::wstring>& arguments)
    -> std::expected<std::uint32_t, std::string>;

// 后台启动 PowerShell 脚本，供需要等待当前进程退出的更新和恢复任务使用。
auto launch_script(const std::filesystem::path& script_path,
                   const std::vector<std::wstring>& arguments) -> std::expected<void, std::string>;

}  // namespace utils::powershell
