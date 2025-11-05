module;

export module Features.Update.Types;

import std;

export namespace Features::Update::Types {

// GitHub Release Asset - 发布资源
struct GitHubAsset {
  std::string name;
  std::string browser_download_url;
  std::size_t size = 0;
  std::string content_type;
};

// GitHub Release - 发布信息
struct GitHubRelease {
  std::string tag_name;             // 版本标签
  std::string name;                 // 发布名称
  std::string body;                 // 发布说明
  bool prerelease = false;          // 是否为预发布版本
  std::vector<GitHubAsset> assets;  // 发布资源文件
};

// === 响应类型定义 ===
// Updater模块的公共API响应类型

// 检查更新响应结果
struct CheckUpdateResult {
  bool has_update;             // 是否有可用更新
  std::string latest_version;  // 最新版本
  std::string download_url;    // 下载链接
  std::string changelog;       // 更新日志
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