#pragma once

#include "vendor/std.hpp"

#include "vendor/asio.hpp"

#include "features/adb_mode/capture_protocol.hpp"
#include "features/adb_mode/types.hpp"
#include "utils/process/process.hpp"

namespace features::adb_mode::session {

// 一个完整的 ADB 设备会话：统管 ADB 连接、Android 捕获服务进程、端口转发和协议通信
struct DeviceSession {
  AdbConnectionConfig config;
  bool connection_owned = false;
  Resolution physical_display;
  Resolution current_display;

  std::unique_ptr<asio::io_context> io_context;
  std::unique_ptr<asio::ip::tcp::socket> socket;
  utils::process::ChildProcess server_process;
  std::filesystem::path server_stderr_file;
  std::string socket_name;
  std::uint16_t local_port = 0;
  std::uint32_t next_request_id = 1;
  mutable std::mutex metadata_mutex;
  std::mutex protocol_mutex;

  ~DeviceSession();
};

// 建立完整设备会话：连接设备 → 查询初始分辨率 → 推送 JAR 服务 → 启动服务端 → 建立 socket 传输
auto open(const AdbConnectionConfig& config)
    -> std::expected<std::shared_ptr<DeviceSession>, std::string>;

// 关闭设备会话：停传输与服务 → 恢复初始分辨率 → 断开自建连接 → 释放会话指针
auto close(std::shared_ptr<DeviceSession>& session) -> std::expected<void, std::string>;

// 恢复物理显示尺寸：重置 wm size → 更新当前会话尺寸快照
auto restore_display(DeviceSession& session) -> std::expected<void, std::string>;

// 设置设备显示尺寸：校验参数 → 执行 wm size override → 更新当前会话尺寸快照
auto set_display(DeviceSession& session, const Resolution& target)
    -> std::expected<void, std::string>;

// 请求单张屏幕截图并保存至文件：发请求帧 → 读响应帧 → 校验魔数 → 原子写盘
auto screenshot_to_file(DeviceSession& session, const std::filesystem::path& output_path,
                        AdbScreenshotFormat format) -> std::expected<void, std::string>;

}  // namespace features::adb_mode::session
