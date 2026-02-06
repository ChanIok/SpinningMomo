module;

export module Features.Update.Types;

import std;

export namespace Features::Update::Types {

// === 响应类型定义 ===
// Update模块的公共API响应类型

// 检查更新响应结果
struct CheckUpdateResult {
  bool has_update;              // 是否有可用更新
  std::string latest_version;   // 最新版本
  std::string current_version;  // 当前版本
};

// 下载更新响应结果
struct DownloadUpdateResult {
  std::filesystem::path file_path;  // 下载的文件路径
  std::string message;              // 结果消息
};

// 安装更新请求参数
struct InstallUpdateParams {
  bool restart = true;  // 安装后是否重启程序
};

// 安装更新响应结果
struct InstallUpdateResult {
  std::string message;  // 结果消息
};

}  // namespace Features::Update::Types
