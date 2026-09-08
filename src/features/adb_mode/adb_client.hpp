#pragma once

#include "vendor/std.hpp"

#include "features/adb_mode/types.hpp"
#include "utils/process/process.hpp"

namespace features::adb_mode::adb {

// 解析用户指定的 ADB 路径。
auto resolve_executable(std::string_view configured_path)
    -> std::expected<std::filesystem::path, std::string>;

// 执行 ADB 命令；只把启动失败/超时视为执行器错误，非零退出码交给调用方解释。
auto run(const AdbConnectionConfig& config, const std::vector<std::wstring>& arguments,
         std::chrono::milliseconds timeout = std::chrono::seconds(15))
    -> std::expected<utils::process::CommandResult, std::string>;

// 为目标设备补上 -s 序列号并执行 ADB 命令，非零退出码直接转换为错误。
auto run_on_device(const AdbConnectionConfig& config, std::string_view serial,
                   const std::vector<std::wstring>& arguments,
                   std::chrono::milliseconds timeout = std::chrono::seconds(15))
    -> std::expected<utils::process::CommandResult, std::string>;

// 执行 adb devices -l 并把标准输出解析成设备列表。
auto list_devices(const AdbConnectionConfig& config)
    -> std::expected<std::vector<AdbDevice>, std::string>;

// 连接配置中的 host:port（如有需要），并返回设备序列号和连接所有权。
auto connect(const AdbConnectionConfig& config) -> std::expected<AdbConnectionResult, std::string>;

// 主动尝试连接指定的 endpoint (例如 127.0.0.1:16384 或 192.168.1.100:5555)。
auto connect_endpoint(const AdbConnectionConfig& config, std::string_view endpoint,
                      std::chrono::milliseconds timeout = std::chrono::seconds(5))
    -> std::expected<void, std::string>;

// 断开本模块建立的 TCP ADB 连接。
auto disconnect(const AdbConnectionConfig& config, std::string_view serial)
    -> std::expected<void, std::string>;

}  // namespace features::adb_mode::adb
