module;

export module Core.State.AppInfo;

import std;

namespace Core::State::AppInfo {

// 应用基本信息结构
export struct AppInfoState {
  // 版本信息
  std::string version;
  unsigned int major_version = 0;
  unsigned int minor_version = 0;
  unsigned int patch_version = 0;
  unsigned int build_number = 0;
  
  // 系统信息
  std::string os_name;
  unsigned int os_major_version = 0;
  unsigned int os_minor_version = 0;
  unsigned int os_build_number = 0;
  
  // 捕获支持状态 (Windows 10 1903 build 18362 or later)
  bool is_capture_supported = false;
};

}  // namespace Core::State::AppInfo