module;

export module Utils.System;

import std;

namespace Utils::System {

// Windows 系统版本信息结构体
export struct WindowsVersionInfo {
  unsigned long major_version = 0;
  unsigned long minor_version = 0;
  unsigned long build_number = 0;
  unsigned long platform_id = 0;
};

// 获取当前 Windows 系统版本信息
export [[nodiscard]] auto get_windows_version() noexcept
    -> std::expected<WindowsVersionInfo, std::string>;

// 根据 WindowsVersionInfo 获取系统名称
export [[nodiscard]] auto get_windows_name(const WindowsVersionInfo& version) noexcept
    -> std::string;

// 检测当前进程是否以管理员权限运行
export [[nodiscard]] auto is_process_elevated() noexcept -> bool;

// 以管理员权限重启当前应用程序
// 返回 true 表示成功启动新进程（当前进程应退出）
// 返回 false 表示用户取消或启动失败
export [[nodiscard]] auto restart_as_elevated(const wchar_t* arguments = nullptr) noexcept -> bool;

}  // namespace Utils::System
