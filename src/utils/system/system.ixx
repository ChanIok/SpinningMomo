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
export [[nodiscard]] auto get_windows_name(const WindowsVersionInfo& version) noexcept -> std::string;

}  // namespace Utils::System